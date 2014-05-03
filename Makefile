CC = gcc
OUT=main
JANSSON_OBJ?=jansson/src/dump.o jansson/src/error.o jansson/src/hashtable.o jansson/src/load.o jansson/src/strbuffer.o jansson/src/utf.o jansson/src/value.o jansson/src/variadic.o
HTTP_PARSER_OBJS?=http-parser/http_parser.o

CFLAGS=-O0 -ggdb -Wall -Wextra -I. -I/usr/local/Cellar/libevent/2.0.21/include -Ijansson/src -Ihttp-parser -I/data/src/php-5.3.27 -I/data/src/php-5.3.27/main -I/data/src/php-5.3.27/Zend -I/data/src/php-5.3.27/TSRM
LDFLAGS=-L/usr/local/Cellar/libevent/2.0.21/lib -levent_core -pthread -L/data/src/php-5.3.27/libs -lphp5

DEPS=$(JANSSON_OBJ) $(HTTP_PARSER_OBJS)
OBJS=main.o cmd.o worker.o slog.o server.o http.o client.o conf.o $(DEPS)

all: $(OUT) Makefile

$(OUT): $(OBJS) Makefile
	$(CC) -o $(OUT) $(OBJS) $(LDFLAGS)

%.o: %.c %.h Makefile
	$(CC) -c $(CFLAGS) -o $@ $<

%.o: %.c Makefile
	$(CC) -c $(CFLAGS) -o $@ $<

clean:
	rm -f $(OBJS) $(OUT)
