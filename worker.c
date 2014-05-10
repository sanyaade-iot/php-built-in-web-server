/* worker.c */
#include "worker.h"
#include "client.h"
#include "http.h"
#include "cmd.h"
#include "slog.h"
#include "conf.h"
#include "server.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <event2/event.h>
#include <string.h>

struct worker *worker_new(struct server *s) {
    int ret;
    struct worker *w = calloc(1, sizeof(struct worker));
    w->s = s;

    /* setup communication link */
    ret = pipe(w->link);
    (void) ret;

    return w;
}

void worker_can_read(int fd, short event, void *p) {
    struct http_client *c = p;
    int ret, nparsed;

    (void) fd;
    (void) event;

    ret = http_client_read(c);
    if (ret <= 0) {
        if ((client_error_t) ret == CLIENT_DISCONNECTED) {
            return;
        } else if (c->failed_alloc || (client_error_t) ret == CLIENT_OOM) {
            slog(c->w->s, LOG_DEBUG, "503", 3);
            http_send_error(c, 503, "Service Unavailable");
            return;
        }
    }

    /* run parser */
    nparsed = http_client_execute(c);

    if (c->failed_alloc) {
        slog(c->w->s, LOG_DEBUG, "503", 3);
        http_send_error(c, 503, "Service Unavailable");
    } else if (nparsed != ret) {
        slog(c->w->s, LOG_DEBUG, "400", 3);
        http_send_error(c, 400, "Bad Request");
    } else if (c->request_sz > MAX_REQUEST_SIZE) {
        slog(c->w->s, LOG_DEBUG, "413", 3);
        http_send_error(c, 413, "Request Entity Too Large");
    }

    if (c->broken) { /* terminate client */
        http_client_free(c);
    } else {
        /* start monitoring input again */
        worker_monitor_input(c);
    }
}

/**
 * Monitor client FD for possible reads.
 */
void worker_monitor_input(struct http_client *c) {
    struct event *ev = event_new(c->w->base, c->fd, EV_READ, worker_can_read, (void*) c);
    event_add(ev, NULL);
}

/**
 * Called when a client is sent to this worker
 */
static void worker_on_new_client(int pipefd, short event, void *ptr) {
    struct http_client *c;
    unsigned long addr;

    (void) event;
    (void) ptr;

    /* Get client from messaging pipe */
    int ret = read(pipefd, &addr, sizeof(addr));
    if (ret == sizeof(addr)) {
        c = (struct http_client *) addr;

        /* monitor client for input */
        worker_monitor_input(c);
    }
}

static void* worker_main(void *p) {
    struct worker *w = p;
    struct event *ev;

    /* setup libevent */
    w->base = event_base_new();

    /* monitor pipe link */
    ev = event_new(w->base, w->link[0], EV_READ | EV_PERSIST, worker_on_new_client, (void*) w);
    event_add(ev, NULL);

    /* loop */
    event_base_dispatch(w->base);

    return NULL;
}

void worker_start(struct worker *w) {
    pthread_create(&w->thread, NULL, worker_main, w);
}

/**
 * Queue new client to process
 */
void worker_add_client(struct worker *w, struct http_client *c) {
    /* write into pipe link */
    unsigned long addr = (unsigned long) c;
    int ret = write(w->link[1], &addr, sizeof(addr));
    (void) ret;
}

/**
 * Called when a client has finished reading input and can create a cmd
 */
void worker_process_client(struct http_client *c) {
    /* check that the command can executed */
    struct worker *w = c->w;
    cmd_response_t ret = CMD_PARAM_ERROR;

    slog(w->s, LOG_DEBUG, c->path, c->path_sz);

    if (!c->match_server) {
        slog(w->s, LOG_DEBUG, "404", 3);
        http_send_error(c, 404, "Not Found");
        return;
    }

    switch (c->parser.method) {
        case HTTP_GET:
            ret = cmd_run(c->w, c, c->path, c->path_sz, NULL, 0);
            break;
        case HTTP_POST:
            ret = cmd_run(c->w, c, c->body, c->body_sz, NULL, 0);
            break;
        default:
            slog(w->s, LOG_DEBUG, "405", 3);
            http_send_error(c, 405, "Method Not Allowed");
            return;
    }

    switch (ret) {
        case CMD_PARAM_ERROR:
            slog(w->s, LOG_DEBUG, "403", 3);
            /* http_send_error(c, 403, "Forbidden"); */
            break;
        default:
            break;
    }
}
