#include "csocket.h"
#include "websocket.h" /* TODO: REMOVE THIS LINE*/

#ifndef CSOCKET_QUEUE_SIZE
#define CSOCKET_QUEUE_SIZE 32
#endif

#ifndef CSOCKET_LIST_SIZE
#define CSOCKET_LIST_SIZE 32
#endif

#define HTONS(n) (((((unsigned short)(n) & 0xFF)) << 8) | (((unsigned short)(n) & 0xFF00) >> 8))
#define NTOHS(n) (((((unsigned short)(n) & 0xFF)) << 8) | (((unsigned short)(n) & 0xFF00) >> 8))

#define HTONL(n) (((((unsigned long)(n) & 0xFF)) << 24) | \
                  ((((unsigned long)(n) & 0xFF00)) << 8) | \
                  ((((unsigned long)(n) & 0xFF0000)) >> 8) | \
                  ((((unsigned long)(n) & 0xFF000000)) >> 24))

#define NTOHL(n) (((((unsigned long)(n) & 0xFF)) << 24) | \
                  ((((unsigned long)(n) & 0xFF00)) << 8) | \
                  ((((unsigned long)(n) & 0xFF0000)) >> 8) | \
                  ((((unsigned long)(n) & 0xFF000000)) >> 24))

static sem_t csocket_semaphore, csocket_mutex, csocket_mutex_csocket_fd;
static int csocket_list[CSOCKET_LIST_SIZE] = {0};
static int csocket_list_count = 0;
static int csocket_queue[CSOCKET_QUEUE_SIZE] = {0};
static int csocket_queue_start = 0;
static int csocket_queue_end = 0;
static int csocket_queue_count = 0;

static void csocket_queue_push(int value) {
	if (csocket_queue_end >= CSOCKET_QUEUE_SIZE) csocket_queue_end = 0;
	
	if (csocket_queue[csocket_queue_end] == 0) {
		csocket_queue[csocket_queue_end] = value;
		csocket_queue_end++;
		csocket_queue_count++;
	}
}

static int csocket_queue_pop() {
	int value;
	
	if (csocket_queue_start >= CSOCKET_QUEUE_SIZE) csocket_queue_start = 0;
	
	if (csocket_queue[csocket_queue_start] != 0) {
		value = csocket_queue[csocket_queue_start];
		csocket_queue[csocket_queue_start] = 0;
		csocket_queue_start++;
		csocket_queue_count--;
		return value;
	}
	
	return -1;
}

static int csocket_list_insert(int value) {
	int i;
	for (i=0; i<CSOCKET_LIST_SIZE; i++) {
		if (csocket_list[i] == 0) {
			csocket_list[i] = value;
			csocket_list_count++;
			return 1;
		}
	}
	
	return 0;
}

static int csocket_list_remove(int value) {
	int i;
	for (i=0; i<CSOCKET_LIST_SIZE; i++) {
		if (csocket_list[i] == value) {
			csocket_list[i] = 0;
			csocket_list_count--;
			return 1;
		}
	}
	
	return 0;
}

static void *csocket_scheduler(void (*on_request)(int)) {
	int sock;
	
	while (1) {
		sem_wait(&csocket_semaphore);
		
		sem_wait(&csocket_mutex);
		sock = csocket_queue_pop();
		csocket_list_insert(sock);
		sem_post(&csocket_mutex);
		
		on_request(sock);
		csocket_close(sock);
		
		sem_wait(&csocket_mutex);
		csocket_list_remove(sock);
		sem_post(&csocket_mutex);
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
	
	if (sem_init(&csocket_mutex_csocket_fd, 0, 1) != 0) {
		fprintf(stderr, "ERROR of csocket_mutex at sem_init()\n");
		return -1;
	}
	
	for (i=0; i<CSOCKET_QUEUE_SIZE; i++) {
		if ((err = pthread_create(&thread, NULL, (void* (*)(void*))csocket_scheduler, (void*)on_request)) != 0) {
			fprintf(stderr, "ERROR %i at pthread_create()\n", err);
			return -1;
		}
		pthread_detach(thread);
	}
	
	while (1) {
		client = accept(sock, &addr, &addrsize);
		
		if (client != -1) {
			csocket_queue_push(client);
			sem_post(&csocket_semaphore);
		}
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
	
	read = calloc(1, max);
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
		fprintf(stderr, "ERROR %i at getpeername();\n", CSOCKET_GET_ERROR());
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

int csocket_parse_request(char *request, char **method, char **path, char **headers, char **body) {
	int i = 0, size = strlen(request);
	
	if (path) *path = NULL;
	
	if (method) *method = request;
	
	for (i=0; i<size; i++) {
		if (request[i] == ' ') {
			request[i] = 0;
			if (path) if (*path == NULL) *path = request + i + 1;
		}
		else if (strncmp(request + i - 2, "\r\n", sizeof("\r\n")-1) == 0) break;
	}
	if (i >= size) return 1;
	
	if (headers) *headers = request + i;
	
	for (i=i; i<size; i++) {
		if (strncmp(request + i, "\r\n\r\n", sizeof("\r\n\r\n")-1) == 0) break;
	}
	if (i >= size) return 1;
	request[i] = 0;
	
	if (body) *body = request + i + (sizeof("\r\n\r\n")-1);
	
	return 0;
}

int csocket_parse_response(char *response, char **statusline, char **headers, char **body) {
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
	if (i >= size) return 1;
	
	if (headers) *headers = response + i;
	
	for (i=i; i<size; i++) {
		if (strncmp(response + i, "\r\n\r\n", sizeof("\r\n\r\n")-1) == 0) break;
	}
	if (i >= size) return 1;
	response[i] = 0;
	
	if (body) *body = response + i + (sizeof("\r\n\r\n")-1);
	
	return 0;
}

int csocket_parse_headers(
	char *headers,
	int (*on_header)(char *name, char *value, void *userdata),
	void *userdata) {
	
	int i = 0, size = strlen(headers), ret;
	char *name, *value;
	
	name = headers;
	
	for (i=0; i<size; i++) {
		if (strncmp(headers + i, ":", sizeof(":")-1) == 0) {
			headers[i] = 0;
			value = headers + i + 2;
		}
		else if (strncmp(headers + i, "\r\n", sizeof("\r\n")-1) == 0) {
			memset(headers + i, 0, 2);
			if ((ret = on_header(name, value, userdata)) != 0) return ret;
			name = headers + i + 2;
		}
	}
	
	if ((ret = on_header(name, value, userdata)) != 0) return ret;
	
	return 0;
}

int csocket_parse_urlencoded(
	char *urlencoded,
	int (*on_urlencoded)(char *name, char *value, void *userdata),
	void *userdata) {
	
	int i = 0, size = strlen(urlencoded), ret;
	char *name, *value;
	
	name = urlencoded;
	
	for (i=0; i<size; i++) {
		if (strncmp(urlencoded + i, "=", sizeof("=")-1) == 0) {
			urlencoded[i] = 0;
			value = urlencoded + i + 1;
		}
		else if (strncmp(urlencoded + i, "&", sizeof("=")-1) == 0) {
			urlencoded[i] = 0;
			if ((ret = on_urlencoded(name, value, userdata))) return ret;
			name = urlencoded + i + 1;
		}
	}
	
	if ((ret = on_urlencoded(name, value, userdata))) return ret;
	
	return 0;
}

int csocket_parse_multipart(
	char *multipart,
	int (*on_multipart)(char *name, char *filename, char *value, int valuesize, void *userdata),
	void *userdata) {
	
	int i = 0, boundarysize, ret;
	char *name = NULL, *filename = NULL, *value = NULL, *boundary, *aux;
	
	boundary = multipart;
	
	if ((aux = strstr(multipart, "\r\n")) == NULL) return 1;
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
					if ((ret = on_multipart(name, filename, value, multipart + i - value - 2, userdata))) return ret;
					if (strncmp(multipart + i + boundarysize, "--", sizeof("--")-1) == 0) return 1;
					filename = NULL;
					break;
				}
				i++;
			}
		}
		i++;
	}
	
	return 0;
}

void csocket_ws_write(int sock, char fin, char opcode, char *data, long int data_size, int key) {
	
	int i;
	char byte, length;
	
	/* set fin and opcode. ignore reserved values */
	byte = ((!!fin) << 7) | opcode;
	csocket_write(sock, &byte, sizeof(char));
	
	/* determine length value */
	if (data_size <= 125) length = data_size;
	else if (data_size <= 0x7FFF) length = 126;
	else length = 127;
	
	/* set mask and length */
	byte = ((!!key) << 7) | length;
	csocket_write(sock, &byte, sizeof(char));	
	
	/* set extra length if needed */
	if (length == 126) {
		data_size = HTONS((short)data_size);
		csocket_write(sock, (char*)&data_size, sizeof(short int));
		data_size = HTONS((short)data_size);
	}
	else if (length == 127) {
		data_size = HTONL(data_size);
		csocket_write(sock, (char*)&data_size, sizeof(long int));
		data_size = HTONL(data_size);
	}
	
	/* encode if key */
	if (!!key) {
		csocket_write(sock, (char*)&key, sizeof(int));
		
		if (data) {
			for (i=0; i<data_size; i++) {
				data[i] = data[i] ^ (char)(key >> (8 * (i % 4)));
			}	
		}
	}
	
	/* set data */
	if (data) csocket_write(sock, data, data_size);
}

int csocket_ws_read(int sock, char *buffer, int size) {
	long int length;
	int key, i;
	char opcode;
	
	/* skip fin and opcode. */
	csocket_read(sock, buffer, sizeof(char));
	opcode = *buffer & 0x7f;
	if (opcode == 0x8) return 0x8; /* close */
	if (opcode == 0x9) return 0x9; /* ping */
	if (opcode == 0xA) return 0xA; /* pong */
	
	/* get length and mask */
	csocket_read(sock, buffer, sizeof(char));
	length = *buffer & 0x7f;
	key = *buffer >> 7;
	
	/* get extra length if is there */
	if (length == 126) {
		csocket_read(sock, (char*)&length, sizeof(short int));
		length = NTOHS((short)length);
	}
	else if (length == 127) {
		csocket_read(sock, (char*)&length, sizeof(long int));
		length = NTOHL(length);
	}
	
	if (size < length) return 1;
	
	if (!key) {
		csocket_read(sock, buffer, length);
		return 0;
	}
	
	csocket_read(sock, (char*)&key, sizeof(int));
	csocket_read(sock, buffer, length);
	
	for (i=0; i<length; i++) {
		buffer[i] = buffer[i] ^ (char)(key >> (8 * (i % 4)));
	}
	
	buffer[i] = 0;
	
	return 0;
}

void csocket_ws_handshake(int sock, char *Sec_WebSocket_Key) {
	char temp[128], digest[20];
	const char *const UUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
	
	strncpy(temp, Sec_WebSocket_Key, sizeof(temp)-sizeof(UUID)-1);
	strcat(temp, UUID);
	sha1digest((unsigned char *)digest, NULL, (unsigned char *)temp, strlen(temp));
	memset(temp, 0, sizeof(temp));
	base64_encode((unsigned char *)digest, sizeof(digest), temp);
	
	csocket_begin_response(sock, "101 Switching Protocols");
	csocket_header(sock, "Sec-WebSocket-Accept", temp);
	csocket_header(sock, "Connection", "Upgrade");
	csocket_header(sock, "Upgrade", "websocket");
	csocket_body(sock, "");
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