/* server.h */
#ifndef SERVER_H
#define SERVER_H

#include <event2/event.h>
#include <pthread.h>

struct worker;
struct conf;

struct server {
    int fd;

    struct event_base *base;

    struct conf *cfg;

    /* worker thread */
    struct worker **w;
    int next_worker;

    /* log lock */
    struct {
        pid_t self;
        int fd;
    } log;
};

struct server *server_new(const char *cfg_file);
int server_start(struct server *s);

#endif /* SERVER_H */
