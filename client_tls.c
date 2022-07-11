#include "./csocket.h"

int main(void) {

	char buf[2048] = {0};

	SSL *ssl = csocket_secure_connect("google.com", "443");
	
	csocket_secure_write(ssl, "GET / HTTP/1.1\r\n\r\n");

	csocket_secure_read(ssl, buf, sizeof(buf));

	printf("%s\n", buf);

	return 0;
}