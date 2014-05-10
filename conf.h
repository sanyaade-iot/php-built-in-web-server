/* conf.h */
#ifndef CONF_H
#define CONF_H

#include "util.h"
#include "slog.h"

#include <stdbool.h>

//config struct defined here
struct rewrite_cfg {
    bool engine;
    bool exist;
    struct str_array rules;
};

struct server_cfg {
    char *root;
    bool autoindex;
    bool proxy;
    bool facade;
    char **error_page;
    char **index;
    char *server_name;
    char *fastcgi_pass;
    int listen;
    char *auth_basic;
    char *auth_basic_user_file;
    struct rewrite_cfg rewrite;
    struct str_array upstream;
};

struct events_cfg{
    int worker_connections;
};

struct http_cfg{
    bool sendfile;
    bool tcp_nopush;
    bool tcp_nodelay;
    int keepalive_timeout;
    int types_hash_max_size;
    bool server_tokens;
    int server_names_hash_bucket_size;
    char *access_log;
    char *error_log;
    char *default_type;
    bool gzip;
    char *gzip_disable;
    bool gzip_vary;
    char *gzip_proxied;
    int gzip_comp_level;
    char *gzip_http_version;
    char **gzip_types;
    struct server_cfg **servers;
};

struct conf {
    char *user;
    int worker_processes;
    char *pid;
    char *mimefile;
    struct events_cfg events;
    struct http_cfg http;

    int daemonize;
    char *logfile;
    log_level verbosity;
};

struct conf *conf;

struct conf *conf_read(const char *filename);
void conf_free(struct conf *conf);

#endif /* CONF_H */
