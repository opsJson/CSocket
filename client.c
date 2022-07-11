#include "./csocket.h"

int main(void) {

	char buf[2048] = {0};

	int sock = csocket_connect("google.com", "80");

	csocket_write(sock, "GET / HTTP/1.1\r\n\r\n");

	csocket_read(sock, buf, sizeof(buf));

	printf("%s\n", buf);

	return 0;
}