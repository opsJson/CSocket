#include "csocket.h"
#define LIST_SIZE 32
int list[LIST_SIZE];
int list_count = 0;

int list_insert(int value) {
	int i;
	for (i=0; i<LIST_SIZE; i++) {
		if (list[i] == 0) {
			list[i] = value;
			list_count++;
			return 1;
		}
	}
	
	return 0;
}

int list_remove(int value) {
	int i;
	for (i=0; i<LIST_SIZE; i++) {
		if (list[i] == value) {
			list[i] = 0;
			list_count--;
			return 1;
		}
	}
	return 0;
}

int on_header(char *name, char *value, void *userdata) {
	int sock = *(int*)userdata;
	
	if (strcmp(name, "Sec-WebSocket-Key") == 0) {
		csocket_ws_handshake_as_server(sock, value);
		return 1;
	}
	return 0;
}

void on_request(int sock) {
	char buffer[2048], *method, *path, *headers, *body;
	int i, ret;
	
	memset(buffer, 0, sizeof(buffer));
	csocket_read(sock, buffer, sizeof(buffer)-1);
	
	csocket_parse_request(buffer, &method, &path, &headers, &body);
	
	if (csocket_parse_headers(headers, on_header, &sock) != 1) return;
	
	printf("new socket: %i\n", sock);
	list_insert(sock);
	
	do {
		memset(buffer, 0, sizeof(buffer));
		ret = csocket_ws_read(sock, buffer, sizeof(buffer)-1);
		
		for (i=0; i<list_count; i++) {
			printf("broadcasting '%s' to: %i\n", buffer, list[i]);
			csocket_ws_write(list[i], 1, 0x1, buffer, strlen(buffer), 0);
		}
		
	} while(ret > 0);
	
	list_remove(sock);
}

int main(void) {
	csocket_listen("localhost", 80, on_request);
	return 0;
}