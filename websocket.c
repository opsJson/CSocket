#include "csocket.h"

int on_header(char *name, char *value, void *userdata) {
	int sock = *(int*)userdata;
	
	if (strcmp(name, "Sec-WebSocket-Key") == 0) {
		csocket_ws_handshake(sock, value);
		return 1;
	}
	return 0;
}

void on_request(int sock) {
	char buffer[2048], *method, *path, *headers, *body;
	int socks[10];
	unsigned i, count;
	
	memset(buffer, 0, sizeof(buffer));
	csocket_read(sock, buffer, sizeof(buffer)-1);
	
	csocket_parse_request(buffer, &method, &path, &headers, &body);
	
	if (csocket_parse_headers(headers, on_header, &sock) != 1) {
		return;
	}
	
	printf("new socket: %i\n", sock);
	
	while (1) {
		memset(buffer, 0, sizeof(buffer));
		csocket_ws_read(sock, buffer, sizeof(buffer)-1);
		
		count = csocket_fd(socks, 10);
		for (i=0; i<count; i++) {
			printf("broadcasting eco to: %i\n", socks[i]);
			csocket_ws_write(socks[i], 1, 0x1, buffer, strlen(buffer), 0);
		}
	}
}

int main(void) {
	csocket_listen("localhost", 80, on_request);
	return 0;
}