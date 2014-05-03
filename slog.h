/* slog.h */
#ifndef SLOG_H
#define SLOG_H

typedef enum {
    LOG_ERROR = 0,
    LOG_WARNING,
    LOG_NOTICE,
    LOG_INFO,
    LOG_DEBUG
} log_level;

struct server;

void slog_reload();
void slog_init(struct server *s);
void slog(struct server *s, log_level level, const char *body, size_t sz);

#endif /* SLOG_H */
