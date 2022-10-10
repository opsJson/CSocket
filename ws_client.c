#include "csocket.h"

int on_header(char *name, char *value, void *userdata) {
	(void)userdata;
	
	printf("HEADER: %s: %s\n", name, value);
	return 0;
}

int main(void) {
	int sock;
	char msg[] = "boa tarde";
	
	if ((sock = csocket_connect("10.0.0.121", "80")) == -1) return 1;
	
	csocket_ws_handshake_as_client(sock, "/", NULL);
	
	csocket_ws_write(sock, 0x1, 0x1, msg, strlen(msg), rand());
	
	csocket_close(sock);
	
	return 0;
}