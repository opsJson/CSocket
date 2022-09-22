#include "csocket.h"

int on_header(char *name, char *value, void *userdata) {
	(void)userdata;
	
	printf("HEADER: %s: %s\n", name, value);
	return 0;
}

int main(void) {
	char *response, *statusline, *headers, *body;
	int sock;
	
	if ((sock = csocket_connect("httpbin.org", "80")) == -1) return 1;
	
	csocket_begin_request(sock, "GET", "/get");
	csocket_header(sock, "Host", "httpbin.org");
	csocket_body(sock, "");
	
	response = csocket_read_all(sock, 1024 * 8);
	
	csocket_parse_response(response, &statusline, &headers, &body);
	
	printf("STATUSLINE: %s\n", statusline);
	csocket_parse_headers(headers, on_header, NULL);
	printf("\n%s\n", body);
	
	free(response);
	
	return 0;
}