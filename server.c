#include "csocket.h"

int on_header(char *name, char *value, void *userdata) {
	(void)userdata;
	
	printf("HEADER: %s: %s\n", name, value);
	return 0;
}

int on_multipart(char *name, char *filename, char *value, int valuesize, void *userdata) {
	(void)userdata;
	
	if (filename) {
		printf("MULTIPART: %s:\tfile %s:\n%.*s\n", name, filename, valuesize, value);
	}
	else {
		printf("MULTIPART: %s: %.*s\n", name, valuesize, value);
	}
	
	return 0;
}

void on_request(int sock) {
	char *buffer, *ip, *method, *path, *headers, *body;
	int port;
	
	csocket_name(sock, &ip, &port);
	printf("NEW CONNECTION: %s:%i\n\n", ip, port);
	
	buffer = csocket_read_all(sock, 20000000);
	printf("%s\n", buffer);
	
	csocket_parse_request(buffer, &method, &path, &headers, &body);
	printf("METHOD: %s\n", method);
	printf("PATH: %s\n", path);
	
	csocket_parse_headers(headers, on_header, NULL);
	csocket_parse_multipart(body, on_multipart, NULL);
	printf("----------------------------------------------\n");
	
	csocket_begin_response(sock, "200 OK");
	csocket_header(sock, "Content-Type", "text/html; charset=utf-8");
	csocket_body(sock, "<h1>Hello World!</h1>");
	
	free(buffer);
}

int main(void) {
	csocket_listen("localhost", 80, on_request);
	return 0;
}