CC=gcc
OUT=main
HTTP_PARSER_OBJS?=http-parser/http_parser.o

CFLAGS=-O0 -ggdb -Wall -Wextra -I. -I/usr/local/Cellar/libevent/2.0.21/include -Ihttp-parser -I/data/src/php-5.3.27 -I/data/src/php-5.3.27/main -I/data/src/php-5.3.27/Zend -I/data/src/php-5.3.27/TSRM
LDFLAGS=-L/usr/local/Cellar/libevent/2.0.21/lib -levent_core -L. -lphp5

DEPS=$(HTTP_PARSER_OBJS)
OBJS=main.o cmd.o worker.o slog.o server.o http.o client.o conf.o embed.o util.o $(DEPS)

all: $(OUT) Makefile

$(OUT): $(OBJS) Makefile
	flex conf.l
	mv lex.yy.c conf.c
	$(CC) -o $(OUT) $(OBJS) $(LDFLAGS)

%.o: %.c %.h Makefile
	$(CC) -c $(CFLAGS) -o $@ $<

%.o: %.c Makefile
	$(CC) -c $(CFLAGS) -o $@ $<

clean:
	rm -f $(OBJS) $(OUT)
