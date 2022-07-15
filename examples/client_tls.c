#include "./csocket.h"

int main(void) {

	char buf[2048] = {0};
	char *request =
		"GET / HTTP/1.1\r\n"
		"Host: www.google.com"
		"\r\n"
		""
		"\r\n";

	SSL *ssl = csocket_secure_connect("google.com", "443");
	
	csocket_secure_write(ssl, request, strlen(request));

	csocket_secure_read(ssl, buf, sizeof(buf));

	printf("%s\n", buf);
	
	csocket_secure_close(ssl);

	return 0;
}
