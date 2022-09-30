CC=gcc
CFLAGS=-Wall -Wextra

ifdef OS
	CFLAGS+=-lws2_32 -std=c89
else
	ifeq ($(shell uname), Linux)
		CFLAGS+=-std=gnu99 -pthread
	endif
endif

all: server client websocket

server: server.c csocket.c
	$(CC) server.c csocket.c $(CFLAGS) -o server

client: client.c csocket.c
	$(CC) client.c csocket.c $(CFLAGS) -o client

websocket: websocket.c csocket.c
	$(CC) websocket.c csocket.c $(CFLAGS) -o websocket

lib: csocket.c
	$(CC) csocket.c $(CFLAGS) -c -o csocket
	ar rcs csocket.lib csocket
	rm csocket

lib32: csocket.c
	$(CC) csocket.c $(CFLAGS) -c -o csocket -m32
	ar rcs csocket.lib csocket
	rm csocket