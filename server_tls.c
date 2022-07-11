#include "./csocket.h"

void onRequestSecure(SSL *ssl) {
	char buffer[2048] = {0};

	csocket_secure_read(ssl, buffer, sizeof(buffer));

	printf("%s\n", buffer);

	csocket_secure_write(ssl,
		"HTTP/1.1 200 OK\r\n"
		"Content-Length: 5\r\n\r\n"
		"hello!\r\n"
		"\r\n");
}

int main(void) {

	csocket_secure_listen("localhost", "8080", onRequestSecure, "./cert.pem", "./key.pem");

	return 0;
}
