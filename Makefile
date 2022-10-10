CC=gcc
CFLAGS=-Wall -Wextra

ifdef OS
	CFLAGS+=-lws2_32 -std=c89
else
	ifeq ($(shell uname), Linux)
		CFLAGS+=-std=gnu99 -pthread
	endif
endif

all: server client ws_server ws_client lib

server: server.c csocket.c
	$(CC) server.c csocket.c $(CFLAGS) -o server

client: client.c csocket.c
	$(CC) client.c csocket.c $(CFLAGS) -o client

ws_server: ws_server.c csocket.c
	$(CC) ws_server.c csocket.c $(CFLAGS) -o ws_server

ws_client: ws_client.c csocket.c
	$(CC) ws_client.c csocket.c $(CFLAGS) -o ws_client

lib: csocket.c
	$(CC) csocket.c $(CFLAGS) -c -o csocket
	ar rcs csocket.lib csocket
	rm csocket

lib32: csocket.c
	$(CC) csocket.c $(CFLAGS) -c -o csocket -m32
	ar rcs csocket.lib csocket
	rm csocket