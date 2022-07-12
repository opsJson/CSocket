#include "./csocket.h"

int main(void) {

	char buf[2048] = {0};
	char *request =
		"GET / HTTP/1.1\r\n"
		"Host: www.google.com"
		"\r\n"
		""
		"\r\n";

	int sock = csocket_connect("google.com", "80");

	csocket_write(sock, request, strlen(request));

	csocket_read(sock, buf, sizeof(buf));

	printf("%s\n", buf);
	
	csocket_close(sock);

	return 0;
}
