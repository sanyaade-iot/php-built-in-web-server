/* conf.h */
#ifndef CONF_H
#define CONF_H

#include <sys/types.h>
#include "slog.h"

struct conf {
    /* HTTP server interface */
    char *http_host;
    short http_port;
    short http_threads;
    size_t http_max_request_size;

    /* daemonize process, off by default */
    int daemonize;
    char *pidfile;

    /* user/group */
    uid_t user;
    gid_t group;

    /* Logging */
    char *logfile;
    log_level verbosity;

    /* Request to serve on "/" */
    char *default_root;
};

struct conf *conf_read(const char *filename);
void conf_free(struct conf *conf);

#endif /* CONF_H */
