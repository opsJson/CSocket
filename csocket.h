#ifndef _C_SOCKET_H_
#define _C_SOCKET_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>

#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
	#ifdef _MSC_VER
		#pragma comment( lib, "ws2_32.lib" )
	#endif
	
	#include <winsock2.h>
	#include <ws2tcpip.h>
	
	#define CSOCKET_GET_ERROR() WSAGetLastError()
	#define CSOCKET_CLOSE(sock) closesocket(sock)
	#define CSOCKET_IOCTL(sock, cmd, argp) ioctlsocket(sock, cmd, argp)
	#define CSOCKET_SLEEP(time) Sleep(time)
#else
	#include <sys/types.h> /* addrinfo */
    #include <sys/socket.h> /* addrinfo */
	#include <netdb.h> /* addrinfo */
	#include <errno.h> /* errno */
	#include <unistd.h> /* close, usleep */
    #include <arpa/inet.h> /* inet_ntoa */
	#include <sys/ioctl.h> /* ioctl */

	#define CSOCKET_GET_ERROR() errno
	#define CSOCKET_CLOSE(sock) close(sock)
	#define CSOCKET_IOCTL(sock, cmd, argp) ioctl(sock, cmd, argp)
	#define CSOCKET_SLEEP(time) usleep(1000 * time)
#endif

/* SOCKET API */
int csocket_connect(const char *host, const char *port);
int csocket_listen(const char *host, const int port, void (*on_request)(int sock));
int csocket_write(int sock, char *message, int size);
int csocket_read(int sock, char *buffer, int size);
char *csocket_read_all(int sock, unsigned int max);
void csocket_close(int sock);
int csocket_name(int sock, char **ip, int *port);

/* HTTP MAKE API */
void csocket_begin_request(int sock, char *method, char *path);
void csocket_begin_response(int sock, char *statusline);
void csocket_header(int sock, char *name, char *value);
void csocket_body_sized(int sock, char *body, int size);
void csocket_body(int sock, char *body);

/* HTTP PARSE API */
int csocket_parse_request(char *request, char **method, char **path, char **headers, char **body);
int csocket_parse_response(char *response, char **statusline, char **headers, char **body);
int csocket_parse_headers(char *headers, int (*on_header)(char *name, char *value, void *userdata), void *userdata);
int csocket_parse_urlencoded(char *urlencoded, int (*on_urlencoded)(char *name, char *value, void *userdata), void *userdata);
int csocket_parse_multipart(char *multipart, int (*on_multipart)(char *name, char *filename, char *value, int valuesize, void *userdata), void *userdata);

/* WEBSOCKET API */
void csocket_ws_handshake_as_client(int sock, char *path, char *protocols);
void csocket_ws_handshake_as_server(int sock, char *Sec_WebSocket_Key);
void csocket_ws_write(int sock, char fin, char opcode, char *data, long int data_size, int key);
int csocket_ws_read(int sock, char *buffer, int size);

/* UTILITIES API */
char *csocket_escape(char *str);

#endif /* _C_SOCKET_H_ */