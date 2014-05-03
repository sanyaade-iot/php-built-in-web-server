/* cmd.h */
#ifndef CMD_H
#define CMD_H

#include <stdlib.h>
#include <sys/queue.h>
#include <sapi/embed/php_embed.h>

struct http_client;
struct server;
struct worker;
struct cmd;

typedef enum {
    CMD_SENT,
	CMD_PARAM_ERROR
} cmd_response_t;

struct cmd {
	int fd;

	/* HTTP data */
	char *mime; /* forced output content-type */
	int mime_free; /* need to free mime buffer */

    int keep_alive;

	/* various flags */
	int started_responding;
	int http_version;

	struct worker *w;
};

struct cmd *cmd_new();
void cmd_free(struct cmd *c);
cmd_response_t cmd_run(struct worker *w, struct http_client *client,
		const char *uri, size_t uri_len,
		const char *body, size_t body_len);
void cmd_send(struct cmd *cmd);
void cmd_setup(struct cmd *cmd, struct http_client *client);

#endif /* CMD_H */
