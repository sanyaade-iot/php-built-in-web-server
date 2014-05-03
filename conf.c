/* conf.c */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>

#include <jansson.h>
#include "conf.h"

struct conf *conf_read(const char *filename) {
    json_t *j;
    json_error_t error;
    struct conf *conf;
    void *kv;

    /* daefults */
    conf = calloc(1, sizeof(struct conf));
    conf->http_host = strdup("0.0.0.0");
    conf->http_port = 80;
    conf->http_max_request_size = 128*1024*1024;
    conf->http_threads = 4;
    conf->user = getuid();
    conf->group = getgid();
    conf->logfile = "pbiws.log";
    conf->verbosity = LOG_NOTICE;
    conf->daemonize = 0;
    conf->pidfile = "pbiws.pid";
    conf->default_root = "/var/www";

    j = json_load_file(filename, 0, &error);
    if (!j) {
        fprintf(stderr, "Error: %s (line %d)\n", error.text, error.line);
        return conf;
    }

    for (kv = json_object_iter(j); kv; kv = json_object_iter_next(j, kv)) {
        json_t *jtmp = json_object_iter_value(kv);

        if(strcmp(json_object_iter_key(kv), "http_host") == 0 && json_typeof(jtmp) == JSON_STRING) {
			free(conf->http_host);
			conf->http_host = strdup(json_string_value(jtmp));
		} else if(strcmp(json_object_iter_key(kv), "http_port") == 0 && json_typeof(jtmp) == JSON_INTEGER) {
			conf->http_port = (short)json_integer_value(jtmp);
		} else if(strcmp(json_object_iter_key(kv), "http_max_request_size") == 0 && json_typeof(jtmp) == JSON_INTEGER) {
			conf->http_max_request_size = (size_t)json_integer_value(jtmp);
		} else if(strcmp(json_object_iter_key(kv), "threads") == 0 && json_typeof(jtmp) == JSON_INTEGER) {
			conf->http_threads = (short)json_integer_value(jtmp);
		} else if(strcmp(json_object_iter_key(kv), "user") == 0 && json_typeof(jtmp) == JSON_STRING) {
			struct passwd *u;
			if((u = getpwnam(json_string_value(jtmp)))) {
				conf->user = u->pw_uid;
			}
		} else if(strcmp(json_object_iter_key(kv), "group") == 0 && json_typeof(jtmp) == JSON_STRING) {
			struct group *g;
			if((g = getgrnam(json_string_value(jtmp)))) {
				conf->group = g->gr_gid;
			}
		} else if(strcmp(json_object_iter_key(kv),"logfile") == 0 && json_typeof(jtmp) == JSON_STRING){
			conf->logfile = strdup(json_string_value(jtmp));
		} else if(strcmp(json_object_iter_key(kv),"verbosity") == 0 && json_typeof(jtmp) == JSON_INTEGER){
			int tmp = json_integer_value(jtmp);
			if(tmp < 0) conf->verbosity = LOG_ERROR;
			else if(tmp > (int)LOG_DEBUG) conf->verbosity = LOG_DEBUG;
			else conf->verbosity = (log_level)tmp;
		} else if(strcmp(json_object_iter_key(kv), "daemonize") == 0 && json_typeof(jtmp) == JSON_TRUE) {
			conf->daemonize = 1;
		} else if(strcmp(json_object_iter_key(kv),"pidfile") == 0 && json_typeof(jtmp) == JSON_STRING){
			conf->pidfile = strdup(json_string_value(jtmp));
		} else if(strcmp(json_object_iter_key(kv), "default_root") == 0 && json_typeof(jtmp) == JSON_STRING) {
			conf->default_root = strdup(json_string_value(jtmp));
		}
    }

    json_decref(j);

    return conf;
}

void conf_free(struct conf *conf) {
    free(conf->http_host);

    free(conf);
}
