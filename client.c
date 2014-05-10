/* client.c */
#include "client.h"
#include "http_parser.h"
#include "http.h"
#include "server.h"
#include "worker.h"
#include "cmd.h"
#include "conf.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#define CHECK_ALLOC(c, prt) if(!(prt)) { c->failed_alloc = 1; return -1; }

void
dump_url (const char *url, const struct http_parser_url *u)
{
  unsigned int i;

  printf("\tfield_set: 0x%x, port: %u\n", u->field_set, u->port);
  for (i = 0; i < UF_MAX; i++) {
    if ((u->field_set & (1 << i)) == 0) {
      printf("\tfield_data[%u]: unset\n", i);
      continue;
    }

    printf("\tfield_data[%u]: off: %u len: %u part: \"%.*s\n",
           i,
           u->field_data[i].off,
           u->field_data[i].len,
           u->field_data[i].len,
           url + u->field_data[i].off);
  }
}

static int http_client_on_url(struct http_parser *p, const char *at, size_t sz) {
    struct http_client *c = p->data;

    CHECK_ALLOC(c, c->path = realloc(c->path, c->path_sz + sz + 1));
    memcpy(c->path + c->path_sz, at, sz);
    c->path_sz += sz;
    c->path[c->path_sz] = 0;

    struct http_parser_url u;

    int result = http_parser_parse_url(c->path, c->path_sz, 0, &u);
    if (result != 0) {
        printf("Parse error : %d\n", result);
    } else {
        printf("Parse ok, result : \n");
        dump_url(c->path, &u);
    }

    return 0;
}

/**
 * Called when the body in parsed.
 */
static int http_client_on_body(struct http_parser *p, const char *at, size_t sz) {
    struct http_client *c = p->data;
    return http_client_add_to_body(c, at, sz);
}

int http_client_add_to_body(struct http_client *c, const char *at, size_t sz) {
    CHECK_ALLOC(c, c->body = realloc(c->body, c->body_sz + sz + 1));
    memcpy(c->body + c->body_sz, at, sz);
    c->body_sz += sz;
    c->body[c->body_sz] = 0;

    return 0;
}

static int http_client_on_header_name(struct http_parser *p, const char *at, size_t sz) {
    struct http_client *c = p->data;
    size_t n = c->header_count;

    /* if we're not adding to the same header name as last time, realloc to add one field. */
    if (c->last_cb != LAST_CB_KEY) {
        n = ++c->header_count;
        CHECK_ALLOC(c, c->headers = realloc(c->headers, n * sizeof(struct http_header)));
        memset(&c->headers[n-1], 0, sizeof(struct http_header));
    }

    /* Add data to the current header name. */
    CHECK_ALLOC(c, c->headers[n-1].key = realloc(c->headers[n-1].key, c->headers[n-1].key_sz + sz + 1));
    memcpy(c->headers[n-1].key + c->headers[n-1].key_sz, at, sz);
    c->headers[n-1].key_sz += sz;
    c->headers[n-1].key[c->headers[n-1].key_sz] = 0;

    c->last_cb = LAST_CB_KEY;

    return 0;
}

static int http_client_on_header_value(struct http_parser *p, const char *at, size_t sz) {
    struct http_client *c = p->data;
    size_t n = c->header_count;

    /* Add data to the current header value. */
    CHECK_ALLOC(c, c->headers[n-1].val = realloc(c->headers[n-1].val, c->headers[n-1].val_sz + sz + 1));
    memcpy(c->headers[n-1].val + c->headers[n-1].val_sz, at, sz);
    c->headers[n-1].val_sz += sz;
    c->headers[n-1].val[c->headers[n-1].val_sz] = 0;

    c->last_cb = LAST_CB_VAL;

    /* react to some values */
    if (strncmp("Expect", c->headers[n-1].key, c->headers[n-1].key_sz) == 0) {
        if (sz == 12 && strncasecmp(at, "100-continue", sz) == 0) {
            /* support HTTP file upload */
            char http100[] = "HTTP/1.1 100 Continue\r\n\r\n";
            int ret = write(c->fd, http100, sizeof(http100)-1);
            (void) ret;
        }
    } else if (strncasecmp("Connection", c->headers[n-1].key, c->headers[n-1].key_sz) == 0) {
        if (sz == 10 && strncasecmp(at, "Keep-Alive", sz) == 0) {
            c->keep_alive = 1;
        }
    } else if (strncmp("Host", c->headers[n-1].key, c->headers[n-1].key_sz) == 0) {
        if (!strchr(c->headers[n-1].val, ':')) {
            CHECK_ALLOC(c, c->headers[n-1].val = realloc(c->headers[n-1].val, c->headers[n-1].val_sz + 3 + 1));
            memcpy(c->headers[n-1].val + c->headers[n-1].val_sz, ":80", 3);
            c->headers[n-1].val_sz += 3;
            c->headers[n-1].val[c->headers[n-1].val_sz] = 0;
        }
    }

    slog(c->w->s, LOG_DEBUG, c->headers[n-1].key, c->headers[n-1].key_sz);
    slog(c->w->s, LOG_DEBUG, c->headers[n-1].val, c->headers[n-1].val_sz);

    return 0;
}

static int http_client_on_headers_complete(struct http_parser *p) {
    struct http_client *c = p->data;
    size_t n = c->header_count;
    char host_port[256];
    size_t i, len;
    int j;

    for (i = 0; i < n; i++) {
        if (strncmp("Host", c->headers[i].key, c->headers[i].key_sz) == 0) {
            j = 0;
            while(c->w->s->cfg->http.servers[j] != NULL) {
                len = sprintf(host_port, "%s:%d", c->w->s->cfg->http.servers[j]->server_name, c->w->s->cfg->http.servers[j]->listen);

                if (c->headers[i].val_sz == len && strncmp(c->headers[i].val, host_port, c->headers[i].val_sz) == 0) {
                    c->match_server = c->w->s->cfg->http.servers[j];
                    break;
                }

                j++;
            }
            break;
        }
    }

    return 0;
}

static int http_client_on_message_complete(struct http_parser *p) {
    struct http_client *c = p->data;

    /* keep-alive detection */
    if (c->parser.http_major == 1 && c->parser.http_minor == 1) { /* 1.1 */
        c->keep_alive = 1;
    }
    c->http_version = c->parser.http_minor;

    /* client info */

    worker_process_client(c);
    http_client_reset(c);

    return 0;
}

struct http_client *http_client_new(struct worker *w, int fd, in_addr_t addr) {
    struct http_client *c = calloc(1, sizeof(struct http_client));

    c->fd = fd;
    c->w = w;
    c->addr = addr;
    c->s = w->s;

    /* parser */
    http_parser_init(&c->parser, HTTP_REQUEST);
    c->parser.data = c;

    /* callbacks */
    /* on_message_begin
     * on_url
     * on_status
     * on_header_field
     * on_header_value
     * on_headers_complete
     * on_body
     * on_message_complete
     */
    c->settings.on_url = http_client_on_url;
    c->settings.on_header_field = http_client_on_header_name;
    c->settings.on_header_value = http_client_on_header_value;
    c->settings.on_headers_complete = http_client_on_headers_complete;
    c->settings.on_body = http_client_on_body;
    c->settings.on_message_complete = http_client_on_message_complete;

    c->last_cb = LAST_CB_NONE;

    return c;
}

void http_client_reset(struct http_client *c) {
    int i;

    /* headers */
    for (i = 0; i < c->header_count; ++i) {
        free(c->headers[i].key);
        free(c->headers[i].val);
    }
    free(c->headers);
    c->headers = NULL;
    c->header_count = 0;

    /* other data */
    free(c->body); c->body = NULL;
    c->body_sz = 0;
    free(c->path); c->path = NULL;
    c->path_sz = 0;
    c->request_sz = 0;

    /* no last known header callback */
    c->last_cb = LAST_CB_NONE;

    /* mark as broken if client doesn't support Keep-Alive. */
    if (c->keep_alive == 0) {
        c->broken = 1;
    }
}

void http_client_free(struct http_client *c) {
    http_client_reset(c);
    free(c->buffer);
    free(c);
}

int http_client_read(struct http_client *c) {
    char buffer[4096];
    int ret;

    ret = read(c->fd, buffer, sizeof(buffer));
    if (ret <= 0) {
        /* broken link, free buffer and client object */

        close(c->fd);
        http_client_free(c);

        return (int) CLIENT_DISCONNECTED;
    }

    /* save what we've just read */
    c->buffer = realloc(c->buffer, c->sz + ret);
    if (!c->buffer) {
        return (int) CLIENT_OOM;
    }

    memcpy(c->buffer + c->sz, buffer, ret);
    c->sz += ret;

    /* keep track of total sent */
    c->request_sz += ret;

    return ret;
}

int http_client_remove_data(struct http_client *c, size_t sz) {
    char *buffer;
    if(c->sz < sz)
        return -1;

    /* replace buffer */
    CHECK_ALLOC(c, buffer = malloc(c->sz - sz));
    memcpy(buffer, c->buffer + sz, c->sz - sz);
    free(c->buffer);
    c->buffer = buffer;
    c->sz -= sz;

    return 0;
}

int http_client_execute(struct http_client *c) {
    int nparsed = http_parser_execute(&c->parser, &c->settings, c->buffer, c->sz);

    /* removed comsumed data, all has been copied already. */
    free(c->buffer);
    c->buffer = NULL;
    c->sz = 0;

    return nparsed;
}

/**
 * Find header value, returns NULL if not found.
 */
const char *client_get_header(struct http_client *c, const char *key) {
    int i;
    size_t sz = strlen(key);

    for (i = 0; i < c->header_count; ++i) {
        if (sz == c->headers[i].key_sz && strncasecmp(key, c->headers[i].key, sz) == 0) {
            return c->headers[i].val;
        }
    }

    return NULL;
}
