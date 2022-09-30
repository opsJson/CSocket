#include "csocket.h"

int on_header(char *name, char *value, void *userdata) {
	int sock = *(int*)userdata;
	
	if (strcmp(name, "Sec-WebSocket-Key") == 0) {
		csocket_ws_handshake(sock, value);
		return 1;
	}
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
	char buffer[2048], *ip, *method, *path, *headers, *body;
	int port, socks[10] = {0};
	unsigned i;
	
	csocket_name(sock, &ip, &port);
	printf("NEW CONNECTION: %s:%i\n\n", ip, port);
	
	memset(buffer, 0, sizeof(buffer));
	csocket_read(sock, buffer, sizeof(buffer)-1);
	
	csocket_parse_request(buffer, &method, &path, &headers, &body);
	
	if (csocket_parse_headers(headers, on_header, &sock) != 1) {
		printf("NOT WEBSOCKET CLIENT, CLOSING %s:%i...\n", ip, port);
		return;
	}
	
	while (1) {
		memset(buffer, 0, 10);
		csocket_ws_read(sock, buffer, sizeof(buffer)-1);
		
		csocket_fd(socks, 10);
		for (i=0; i<10; i++) {
			if (socks[i]) {
				csocket_name(socks[i], &ip, &port);
				printf("broadcasting eco to: %s:%i\n", ip, port);
				csocket_ws_write(socks[i], 1, 0x1, buffer, strlen(buffer), 0);
			}
		}
	}
}

int main(void) {
	csocket_listen("localhost", 80, on_request);
	return 0;
}