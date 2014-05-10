/* server.c */
#include "server.h"
#include "worker.h"
#include "client.h"
#include "conf.h"
#include "util.h"
#include "version.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

static void server_can_accept(int fd, short event, void *ptr);

/**
 * Sets up a non-blocking socket
 */
static int socket_setup(struct server *s, struct server_cfg **servers) {
    int servers_num = 0;
    int i = 0;
    int ports_num = 0;

    // stat server conf num
    while (servers[servers_num++] != NULL);
    servers_num--;

    // stat distinct listen port num
    int *servers_port = (int *)malloc(sizeof(int) * servers_num);
    for(i = 0; i < servers_num; i++) {
        int listen_port = servers[i]->listen;
        if(in_int_array(servers_port, listen_port, ports_num) == -1) {
            servers_port[ports_num++] = listen_port;
        }
    }

    for(i = 0; i < ports_num; i++) {
        int reuse = 1;
        struct sockaddr_in addr;
        int fd, ret;

        memset(&addr, 0, sizeof(addr));

#if defined __BSD__
        addr.sin_len = sizeof(struct sockaddr_in);
#endif
        addr.sin_family = AF_INET;
        addr.sin_port = htons(servers_port[i]);
        addr.sin_addr.s_addr = INADDR_ANY;

        /* create socket */
        fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (-1 == fd) {
            slog(s, LOG_ERROR, strerror(errno), 0);
            free(servers_port);
            return -1;
        }

        /* reuse address if we've bound to it before. */
        if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
            slog(s, LOG_ERROR, strerror(errno), 0);
            free(servers_port);
            return -1;
        }

        /* set socket as non-blocking. */
        ret = fcntl(fd, F_SETFD, O_NONBLOCK);
        if (0 != ret) {
            slog(s, LOG_ERROR, strerror(errno), 0);
            free(servers_port);
            return -1;
        }

        /* bind */
        ret = bind(fd, (struct sockaddr*) &addr, sizeof(addr));
        if (0 != ret) {
            slog(s, LOG_ERROR, strerror(errno), 0);
            free(servers_port);
            return -1;
        }

        /* listen */
        ret = listen(fd, SOMAXCONN);
        if (0 != ret) {
            slog(s, LOG_ERROR, strerror(errno), 0);
            free(servers_port);
            return -1;
        }

        /* set keepalive socket option to do with half connection */
        int keep_alive = 1;
        setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (void *) &keep_alive, sizeof(keep_alive));

        /* start http server */
        struct event *ev = event_new(s->base, fd, EV_READ | EV_PERSIST, server_can_accept, (void*) s);
        ret = event_add(ev, NULL);

        if (ret < 0) {
            slog(s, LOG_ERROR, "Error calling event_add on socket", 0);
            free(servers_port);
            return -1;
        }
    }
    free(servers_port);

    return 0;
}

struct server *server_new(const char *cfg_file) {
    int i;
    struct server *s = calloc(1, sizeof(struct server));

    s->log.fd = -1;
    s->cfg = conf_read(cfg_file);

    /* workers */
    s->w = calloc(s->cfg->worker_processes, sizeof(struct worker*));
    for (i = 0; i < s->cfg->worker_processes; ++i) {
        s->w[i] = worker_new(s);
    }

    return s;
}

static void server_can_accept(int fd, short event, void *ptr) {
    struct server *s = ptr;
    struct worker *w;
    struct http_client *c;
    int client_fd;
    struct sockaddr_in addr;
    socklen_t addr_sz = sizeof(addr);
    char on = 1;
    (void) event;

    /* select worker to send the client to */
    w = s->w[s->next_worker];

    /* accept client */
    client_fd = accept(fd, (struct sockaddr*) &addr, &addr_sz);

    /* make non-blocking */
    ioctl(client_fd, (int) FIONBIO, (char *) &on);

    /* create client and send to worker */
    if (client_fd > 0) {
        c = http_client_new(w, client_fd, addr.sin_addr.s_addr);
        worker_add_client(w, c);

        /* loop over ring of workers */
        s->next_worker = (s->next_worker + 1) % s->cfg->worker_processes;
    } else { /* too many connections */
        slog(s, LOG_NOTICE, "Too many connections", 0);
    }
}

/**
 * Daemonize server.
 * (taken from Redis)
 */
static void server_daemonize(const char *pidfile) {
    int fd;

    if (fork() != 0) exit(0); /* parent exits */
    setsid(); /* create a new session */

    /* Every output goes to /dev/null. */
    if ((fd = open("/dev/null", O_RDWR, 0)) != -1) {
        dup2(fd, STDIN_FILENO);
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        if (fd > STDERR_FILENO) close(fd);
    }

    /* write pidfile */
    if (pidfile) {
        FILE *f = fopen(pidfile, "w");
        if (f) {
            fprintf(f, "%d\n", (int) getpid());
            fclose(f);
        }
    }
}

/* global pointer to the server object, used in signal handlers */
static struct server *__server;

static void server_handle_signal(int id) {
    switch (id) {
        case SIGHUP:
            slog_init(__server);
            break;
        case SIGTERM:
        case SIGINT:
            slog(__server, LOG_INFO, "pbiws terminating", 0);
            exit(0);
            break;
        default:
            break;
    }
}

static void server_install_signal_handlers(struct server *s) {
    __server = s;

    signal(SIGHUP, server_handle_signal);
    signal(SIGTERM, server_handle_signal);
    signal(SIGINT, server_handle_signal);
}

int server_start(struct server *s) {
    int i, ret;

    /* initialize libevent */
    s->base = event_base_new();

    if (s->cfg->daemonize) {
        server_daemonize(s->cfg->pid);

        /* sometimes event mech gets lost on fork */
        if (event_reinit(s->base) != 0) {
            fprintf(stderr, "Error: event_reinit failed after fork");
        }
    }

    /* ignore sigpipe */
#ifdef SIGPIPE
    signal(SIGPIPE, SIG_IGN);
#endif

    slog_init(s);

    /* install signal handlers */
    server_install_signal_handlers(s);

    /* start worker threads */
    for (i = 0; i < s->cfg->worker_processes; ++i) {
        worker_start(s->w[i]);
    }

    /* create socket */
	ret = socket_setup(s, s->cfg->http.servers);
    if(ret < 0) {
		return -1;
	}

    /* dispatch */
    slog(s, LOG_INFO, "pbiws " PBIWS_VERSION " up and running", 0);
    event_base_dispatch(s->base);

    return 0;
}
