#include "csocket.h"

#ifndef CSOCKET_QUEUE_SIZE
#define CSOCKET_QUEUE_SIZE 32
#endif

static sem_t csocket_semaphore, csocket_mutex;
static int csocket_queue[CSOCKET_QUEUE_SIZE];
static int csocket_queue_start = 0;
static int csocket_queue_end = 0;
static int csocket_queue_count = 0;

static void csocket_queue_push(int value) {
	if (csocket_queue_end >= CSOCKET_QUEUE_SIZE) csocket_queue_end = 0;
	
	if (csocket_queue[csocket_queue_end] == 0) {
		csocket_queue[csocket_queue_end] = value;
		csocket_queue_end++;
		csocket_queue_count++;
		sem_post(&csocket_semaphore);
	}
}

static int csocket_queue_pop() {
	int value = -1;
	
	if (csocket_queue_start >= CSOCKET_QUEUE_SIZE) csocket_queue_start = 0;
	
	if (csocket_queue[csocket_queue_start] != 0) {
		value = csocket_queue[csocket_queue_start];
		csocket_queue[csocket_queue_start] = 0;
		csocket_queue_start++;
		csocket_queue_count--;
		return value;
	}
	
	return value;
}

static void *csocket_scheduler(void (*on_request)(int)) {
	int sock;
	
	while (1) {
		sem_wait(&csocket_semaphore);
		
		sem_wait(&csocket_mutex);
		sock = csocket_queue_pop();
		sem_post(&csocket_mutex);
		
		on_request(sock);
		csocket_close(sock);
	}
	
	return NULL;
}

int csocket_connect(const char *host, const char *port) {
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)	
	WSADATA wsa;
#endif
	struct addrinfo *res;
	int sock;
	
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
		fprintf(stderr, "ERROR %i at WSAStartup();\n", CSOCKET_GET_ERROR());
	}
#endif

	if (getaddrinfo(host, port, NULL, &res) != 0) {
		fprintf(stderr, "ERROR %i at getaddrinfo();\n", CSOCKET_GET_ERROR());
		return -1;
	}
	
	if ((sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1) {
		fprintf(stderr, "ERROR %i at sock();\n", CSOCKET_GET_ERROR());
		freeaddrinfo(res);
		return -1;
	}
	
	if (connect(sock, res->ai_addr, res->ai_addrlen) != 0) {
		fprintf(stderr, "ERROR %i at connect();\n", CSOCKET_GET_ERROR());
		freeaddrinfo(res);
		return -1;
	}

	freeaddrinfo(res);
	
	return sock;
}


int csocket_listen(const char *host, const int port, void (*on_request)(int sock)) {
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)	
	WSADATA wsa;
#endif
	struct sockaddr_in server;
	struct sockaddr addr;
	int i, sock, client, err;
	socklen_t addrsize = sizeof(addr);
	pthread_t thread;
	
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
		fprintf(stderr, "ERROR %i at WSAStartup();\n", CSOCKET_GET_ERROR());
	}
#endif

	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		fprintf(stderr, "ERROR %i at sock();\n", CSOCKET_GET_ERROR());
		return -1;
	}
	
	if (strcmp("localhost", host) == 0) {
		server.sin_addr.s_addr = INADDR_ANY;
	}
	else {
		server.sin_addr.s_addr = inet_addr(host);
	}
	
	server.sin_port = htons(port);
	server.sin_family = AF_INET;
	
	if (bind(sock, (struct sockaddr*)&server, sizeof(server)) != 0) {
		fprintf(stderr, "ERROR %i at bind();\n", CSOCKET_GET_ERROR());
		return -1;
	}
	
	if (listen(sock, SOMAXCONN) != 0) {
		fprintf(stderr, "ERROR %i at listen();\n", CSOCKET_GET_ERROR());
		return -1;
	}
	
	if (sem_init(&csocket_semaphore, 0, 0) != 0) {
		fprintf(stderr, "ERROR of csocket_semaphore at sem_init()\n");
		return -1;
	}
	
	if (sem_init(&csocket_mutex, 0, 1) != 0) {
		fprintf(stderr, "ERROR of csocket_mutex at sem_init()\n");
		return -1;
	}
	
	for (i=0; i<CSOCKET_QUEUE_SIZE; i++) {
		if ((err = pthread_create(&thread, NULL, (void* (*)(void*))csocket_scheduler, (void*)on_request)) != 0) {
			fprintf(stderr, "ERROR %i at pthread_create()\n", err);
			return -1;
		}
	}
	
	while (1) {
		client = accept(sock, &addr, &addrsize);
		
		if (client != -1) csocket_queue_push(client);
		else fprintf(stderr, "ERROR %i at accept()\n", CSOCKET_GET_ERROR());
	}
	
	return 0;
}

int csocket_write(int sock, char *message, int size) {
	int sent;
	
	while (size) {
		sent = send(sock, message, size, 0);
		
		if (sent <= 0) return sent;
		
		message += sent;
		size -= sent;
	}
	
	return 1;
}

int csocket_read(int sock, char *buffer, int size) {
	int recved;
	unsigned long iRead;
	
	while (size) {
		recved = recv(sock, buffer, size, 0);
		
		if (recved <= 0) return -1;
		
		CSOCKET_IOCTL(sock, FIONREAD, &iRead);
		if (iRead == 0) {
			CSOCKET_SLEEP(200);
			CSOCKET_IOCTL(sock, FIONREAD, &iRead);
			if (iRead == 0) return 0;
		}
		
		buffer += recved;
		size -= recved;
	}
	
	return 1;
}

char *csocket_read_all(int sock, unsigned int max) {
	char *read, *ptr;
	int ret;
	
	read = malloc(max);
	ptr = read;
	
	do {
		ret = csocket_read(sock, read, max);
		read += ret;
		max -= ret;
		
	} while (ret == 1 && max);
	
	return ptr;
}

void csocket_close(int sock) {
	CSOCKET_CLOSE(sock);
}

int csocket_name(int sock, char **ip, int *port) {
	struct sockaddr_in addr;
	socklen_t addrsize = sizeof(addr);
	
	if (getpeername(sock, (struct sockaddr*)&addr, &addrsize) != 0) {
		fprintf(stderr, "ERROR %i at listen();\n", CSOCKET_GET_ERROR());
		return -1;
	}
	
	if (ip) *ip = inet_ntoa(addr.sin_addr);
	if (port) *port = ntohs(addr.sin_port);
	
	return 0;
}

void csocket_begin_request(int sock, char *method, char *path) {
	csocket_write(sock, method, strlen(method));
	
	if (strchr(path, '/') == NULL) {
		csocket_write(sock, " /", sizeof(" /")-1);
	}
	else {
		csocket_write(sock, " ", sizeof(" ")-1);
	}
	
	csocket_write(sock, path, strlen(path));
	csocket_write(sock, " HTTP/1.1\r\n", sizeof(" HTTP/1.1\r\n")-1);
}

void csocket_begin_response(int sock, char *statusline) {
	csocket_write(sock, "HTTP/1.1 ", sizeof("HTTP/1.1 ")-1);
	csocket_write(sock, statusline, strlen(statusline));
	csocket_write(sock, "\r\n", sizeof("\r\n")-1);
}

void csocket_header(int sock, char *name, char *value) {
	csocket_write(sock, name, strlen(name));
	csocket_write(sock, ": ", sizeof(": ")-1);
	csocket_write(sock, value, strlen(value));
	csocket_write(sock, "\r\n", sizeof("\r\n")-1);
}

void csocket_body_sized(int sock, char *body, int size) {
	csocket_write(sock, "\r\n", sizeof("\r\n")-1);
	csocket_write(sock, body, size);
}

void csocket_body(int sock, char *body) {
	csocket_body_sized(sock, body, strlen(body));
}

void csocket_parse_request(char *request, char **method, char **path, char **headers, char **body) {
	int i = 0, size = strlen(request);
	
	if (path) *path = NULL;
	
	if (method) *method = request;
	
	for (i=0; i<size; i++) {
		if (request[i] == ' ') {
			request[i] = 0;
			if (*path == NULL) *path = request + i + 1;
		}
		else if (strncmp(request + i - 2, "\r\n", sizeof("\r\n")-1) == 0) break;
	}
	
	if (headers) *headers = request + i;
	
	for (i=i; i<size; i++) {
		if (strncmp(request + i, "\r\n\r\n", sizeof("\r\n\r\n")-1) == 0) break;
	}
	request[i] = 0;
	
	if (body) *body = request + i + (sizeof("\r\n\r\n")-1);
}

void csocket_parse_response(char *response, char **statusline, char **headers, char **body) {
	int i = 0, size = strlen(response);
	
	if (statusline) *statusline = NULL;
	
	for (i=0; i<size; i++) {
		if (statusline) {
			if (response[i] == ' ' && *statusline == NULL) {
				*statusline = response + i + 1;
			}
		}
		if (strncmp(response + i - 2, "\r\n", sizeof("\r\n")-1) == 0) {
			response[i-2] = 0;
			break;
		}
	}
	
	if (headers) *headers = response + i;
	
	for (i=i; i<size; i++) {
		if (strncmp(response + i, "\r\n\r\n", sizeof("\r\n\r\n")-1) == 0) break;
	}
	response[i] = 0;
	
	if (body) *body = response + i + (sizeof("\r\n\r\n")-1);
}

void csocket_parse_headers(
	char *headers,
	int (*on_header)(char *name, char *value, void *userdata),
	void *userdata) {
	
	int i = 0, size = strlen(headers);
	char *name, *value;
	
	name = headers;
	
	for (i=0; i<size; i++) {
		if (strncmp(headers + i, ":", sizeof(":")-1) == 0) {
			headers[i] = 0;
			value = headers + i + 2;
		}
		else if (strncmp(headers + i, "\r\n", sizeof("\r\n")-1) == 0) {
			memset(headers + i, 0, 2);
			if (on_header(name, value, userdata) != 0) return;
			name = headers + i + 2;
		}
	}
	
	if (on_header(name, value, userdata) != 0) return;
}

void csocket_parse_urlencoded(
	char *urlencoded,
	int (*on_urlencoded)(char *name, char *value, void *userdata),
	void *userdata) {
	
	int i = 0, size = strlen(urlencoded);
	char *name, *value;
	
	name = urlencoded;
	
	for (i=0; i<size; i++) {
		if (strncmp(urlencoded + i, "=", sizeof("=")-1) == 0) {
			urlencoded[i] = 0;
			value = urlencoded + i + 1;
		}
		else if (strncmp(urlencoded + i, "&", sizeof("=")-1) == 0) {
			urlencoded[i] = 0;
			if (on_urlencoded(name, value, userdata)) return;
			name = urlencoded + i + 1;
		}
	}
	
	if (on_urlencoded(name, value, userdata)) return;
}

void csocket_parse_multipart(
	char *multipart,
	int (*on_multipart)(char *name, char *filename, char *value, int valuesize, void *userdata),
	void *userdata) {
	
	int i = 0, boundarysize;
	char *name = NULL, *filename = NULL, *value = NULL, *boundary, *aux;
	
	boundary = multipart;
	
	if ((aux = strstr(multipart, "\r\n")) == NULL) return;
	*aux = 0;
	boundarysize = strlen(boundary);
	multipart += boundarysize;
	
	while (1) {
		if (strncmp(multipart + i - (sizeof(" name=\"")-1), " name=\"", sizeof(" name=\"")-1) == 0) {
			name = multipart + i;
			
			while (1) {
				if (strncmp(multipart + i, "\"", sizeof("\"")-1) == 0) {
					multipart[i] = 0;
					break;
				}
				i++;
			}
		}
		else if (strncmp(multipart + i - (sizeof("filename=\"")-1), "filename=\"", sizeof("filename=\"")-1) == 0) {
			
			filename = multipart + i;
			
			while (1) {
				if (strncmp(multipart + i, "\"", sizeof("\"")-1) == 0) {
					multipart[i] = 0;
					break;
				}
				i++;
			}
		}
		else if (strncmp(multipart + i - (sizeof("\r\n\r\n")-1), "\r\n\r\n", sizeof("\r\n\r\n")-1) == 0) {
			
			value = multipart + i;
			
			while (1) {
				if (strncmp(multipart + i, boundary, boundarysize) == 0) {
					multipart[i] = 0;
					if (on_multipart(name, filename, value, multipart + i - value - 2, userdata)) return;
					if (strncmp(multipart + i + boundarysize, "--", sizeof("--")-1) == 0) return;
					filename = NULL;
					break;
				}
				i++;
			}
		}
		i++;
	}
}

char *csocket_escape(char *str) {
	unsigned int i, j, count = 0, length = strlen(str);
	char *escaped;
	
	for (i=0; i<length; i++) {
		if (str[i] == '<') count += 3;
		else if (str[i] == '>') count += 3;
		else if (str[i] == '&') count += 4;
		else if (str[i] == '\'') count += 4;
		else if (str[i] == '\"') count += 5;
		/*TODO: SPECIFIC PART:*/
		else if (str[i] == '\n') count += 3;
	}
	
	escaped = calloc(1, strlen(str) + count + 1);
	
	for (i=0,j=0; i<length; i++) {
		if (str[i] == '<') memcpy(&escaped[j], "&lt;", sizeof("&lt;")-1), j+=sizeof("&lt;")-1;
		else if (str[i] == '>') memcpy(&escaped[j], "&gt;", sizeof("&gt;")-1), j+=sizeof("&gt;")-1;
		else if (str[i] == '&') memcpy(&escaped[j], "&amp;", sizeof("&amp;")-1), j+=sizeof("&amp;")-1;
		else if (str[i] == '\'') memcpy(&escaped[j], "&#39;", sizeof("&#39;")-1), j+=sizeof("&#39;")-1;
		else if (str[i] == '\"') memcpy(&escaped[j], "&quot;", sizeof("&quot;")-1), j+=sizeof("&quot;")-1;
		else if (str[i] == '\n') memcpy(&escaped[j], "<br>", sizeof("<br>")-1), j+=sizeof("<br>")-1;
		else escaped[j++] = str[i];
	}
	
	return escaped;
}