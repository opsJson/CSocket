#include "./csocket.h"

bool onRequest(int sock) {
	
	char buffer[2048] = {0};
	char *response =
		"HTTP/1.1 200 OK\r\n"
		"Content-Length: 5\r\n\r\n"
		"hello!\r\n"
		"\r\n";

	csocket_read(sock, buffer, sizeof(buffer));

	printf("%s\n", buffer);

	csocket_write(sock, response, strlen(response));
	
	return true;
}

int main(void) {

	csocket_listen("localhost", "8080", onRequest);

	return 0;
}
