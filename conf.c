/* conf.c */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>

#include "conf.h"
#include "util.h"

struct conf *conf_read(const char *filename) {
    struct conf *conf;

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

    char buf[4096];
    char *line;
    int argc;
    char **argv;

    if (filename) {
        FILE *fp;

        if ((fp = fopen(filename, "r")) == NULL) {
            fprintf(stderr, "Error: can't open config file '%s'\n", filename);
            return conf;
        }

        while(fgets(buf, 4096, fp) != NULL) {
            line = strdup(buf);
            line = sdstrim(line, " \t\r\n");

            if (line[0] == '#' || line[0] == '\0') {
                free(line);
                continue;
            }

            argv = strsplit(line, strlen(line), " ", 1, &argc);
            if (argc == 2) {
                if (strcmp(argv[0], "http_host") == 0) {
                    free(conf->http_host);
                    conf->http_host = strdup(argv[1]);
                } else if (strcmp(argv[0], "http_port") == 0) {
                    conf->http_port = (short) atoi(argv[1]);
                } else if (strcmp(argv[0], "http_max_request_size") == 0) {
                    conf->http_max_request_size = (size_t) atoi(argv[1]);
                } else if (strcmp(argv[0], "threads") == 0) {
                    conf->http_threads = (short) atoi(argv[1]);
                } else if (strcmp(argv[0], "user") == 0) {
                    struct passwd *u;
                    if((u = getpwnam(argv[0]))) {
                        conf->user = u->pw_uid;
                    }
                } else if (strcmp(argv[0], "group") == 0) {
                    struct group *g;
                    if((g = getgrnam(argv[0]))) {
                        conf->group = g->gr_gid;
                    }
                } else if (strcmp(argv[0], "logfile") == 0) {
                    conf->logfile = strdup(argv[1]);
                } else if (strcmp(argv[0], "verbosity") == 0) {
                    int tmp = atoi(argv[1]);
                    if(tmp < 0) conf->verbosity = LOG_ERROR;
                    else if(tmp > (int)LOG_DEBUG) conf->verbosity = LOG_DEBUG;
                    else conf->verbosity = (log_level) tmp;
                } else if (strcmp(argv[0], "daemonize") == 0 && strcmp(argv[1], "true") == 0) {
                    conf->daemonize = 1;
                } else if (strcmp(argv[0], "pidfile") == 0) {
                    conf->pidfile = strdup(argv[1]);
                } else if (strcmp(argv[0], "default_root") == 0) {
                    conf->default_root = strdup(argv[1]);
                }
            }

            strfree(argv, argc);
            free(line);
        }

        fclose(fp);
    }

    return conf;
}

void conf_free(struct conf *conf) {
    free(conf->http_host);

    free(conf);
}
