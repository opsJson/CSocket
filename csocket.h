#ifndef _CSOCKETS_H_
#define _CSOCKETS_H_

#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
	#ifdef _MSC_VER
		#pragma comment( lib, "ws2_32.lib" )
		#pragma comment( lib, "libssl.lib" )
		#pragma comment( lib, "libcrypto.lib" )
	#endif
	#include <winsock2.h>
	#include <ws2tcpip.h>
#else
	#define closesocket close
	#include <netdb.h>
#endif

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <openssl/ssl.h>

int csocket_connect(char *host, char *port) {
	int sock = -1;
	struct addrinfo *res;

#if	defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
		fprintf(stderr, "ERROR: WSAStartup failed.\n");
		return -1;
	}
#endif

	if (getaddrinfo(host, port, NULL, &res) != 0) {
		fprintf(stderr, "ERROR: getaddrinfo failed.\n");
		return -1;
	}

	if ((sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1) {
		fprintf(stderr, "ERROR: socket failed.\n");
		goto error;
	}

	if (connect(sock, res->ai_addr, res->ai_addrlen) != 0) {
		fprintf(stderr, "ERROR: connect failed.\n");
		goto error;
	}

	return sock;

error:

	freeaddrinfo(res);
	return -1;
}

SSL* csocket_secure_connect(char *host, char *port) {
	int sock, err;
	SSL_CTX* ctx;
	SSL* ssl = NULL;

	OpenSSL_add_all_algorithms();

	if ((ctx = SSL_CTX_new(TLS_client_method())) == NULL) {
		fprintf(stderr, "ERROR: SSL_CTX_new failed.\n");
		return NULL;
	}

	if ((ssl = SSL_new(ctx)) == NULL) {
		fprintf(stderr, "ERROR: SSL_new failed.\n");
		return NULL;
	}

	if ((sock = csocket_connect(host, port)) == -1) {
		fprintf(stderr, "ERROR: csocket_connect failed.\n");
		goto error;
	}

	if ((err = SSL_set_fd(ssl, sock)) != 1) {
		fprintf(stderr, "ERROR: SSL_set_fd failed: %s\n", strerror(err));
		goto error;
	}

	if ((err = SSL_connect(ssl)) != 1) {
		fprintf(stderr, "ERROR: SSL_connect failed: %s\n", strerror(err));
		goto error;
	}

	return ssl;
	
error:
	
	SSL_free(ssl);
	return NULL;
}

int csocket_write(int sock, char *msg, int msglen) {
	int sent;

	while (msglen) {
		sent = send(sock, msg, msglen, 0);
		if (sent == -1) return -1;
		msg += sent;
		msglen -= sent;
	}

	return 0;
}

int csocket_secure_write(SSL *ssl, char *msg, int msglen) {
	int sent;

	while (msglen) {
		sent = SSL_write(ssl, msg, msglen);
		if (sent == -1) return -1;
		msg += sent;
		msglen -= sent;
	}

	return 0;
}

int csocket_read(int sock, char *buf, int buflen) {
	return recv(sock, buf, buflen, 0);
}

int csocket_secure_read(SSL *ssl, char *buf, int buflen) {
	return SSL_read(ssl, buf, buflen);
}

void csocket_close(int sock) {
	closesocket(sock);
}

void csocket_secure_close(SSL *ssl) {
	SSL_shutdown(ssl);
	SSL_free(ssl);
}

int csocket_listen(char *host, char *port, bool (*on_request)(int)) {
	int sock, client, addrlen;
	struct addrinfo *res;
	struct sockaddr_in addr;

#if	defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
		fprintf(stderr, "ERROR: WSAStartup failed.\n");
		return -1;
	}
#endif

	if (getaddrinfo(host, port, NULL, &res) != 0) {
		fprintf(stderr, "ERROR: getaddrinfo failed.\n");
		return -1;
	}

	if (strcmp(host, "localhost") == 0) {
		struct sockaddr_in* ptr = (struct sockaddr_in*)res->ai_addr;
		ptr->sin_addr.s_addr = INADDR_ANY;
		ptr->sin_family = AF_INET;
		res->ai_family = AF_INET;
	}

	if ((sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1) {
		fprintf(stderr, "ERROR: socket failed.\n");
		goto error;
	}

	if (bind(sock, res->ai_addr, res->ai_addrlen) != 0) {
		fprintf(stderr, "ERROR: bind failed.\n");
		goto error;
	}

	if (listen(sock, SOMAXCONN) != 0) {
		fprintf(stderr, "ERROR: listen failed.\n");
		goto error;
	}

	freeaddrinfo(res);

	addrlen = sizeof(addr);

	while ((client = accept(sock, (struct sockaddr*)&addr, &addrlen)) != -1) {

		while (1) {
			if (on_request(client)) {
				closesocket(client);
				break;
			}
			
			sleep(1);
		}
	}

	return 0;

error:
	freeaddrinfo(res);
	return -1;
}

int csocket_secure_listen(char *host, char *port, bool (*on_request)(SSL*), char *cert_path, char *key_path) {
	int sock, client, addrlen;
	struct addrinfo* res;
	struct sockaddr_in addr;
	SSL_CTX* ctx;
	SSL* ssl;

#if	defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
		fprintf(stderr, "ERROR: WSAStartup failed.\n");
		return -1;
	}
#endif

	ctx = SSL_CTX_new(TLS_server_method());

	if (SSL_CTX_use_certificate_file(ctx, cert_path, SSL_FILETYPE_PEM) <= 0) {
		fprintf(stderr, "ERROR: SSL_CTX_use_certificate_file failed.\n");
		goto error_ctx;
	}

	if (SSL_CTX_use_PrivateKey_file(ctx, key_path, SSL_FILETYPE_PEM) <= 0) {
		fprintf(stderr, "ERROR: SSL_CTX_use_PrivateKey_file failed.\n");
		goto error_ctx;
	}

	if (getaddrinfo(host, port, NULL, &res) != 0) {
		fprintf(stderr, "ERROR: getaddrinfo failed.\n");
		goto error_ctx;
	}

	if (strcmp(host, "localhost") == 0) {
		struct sockaddr_in* ptr = (struct sockaddr_in*)res->ai_addr;
		ptr->sin_addr.s_addr = INADDR_ANY;
		ptr->sin_family = AF_INET;
		res->ai_family = AF_INET;
	}

	if ((sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1) {
		fprintf(stderr, "ERROR: socket failed.\n");
		goto error_addrinfo;
	}

	if (bind(sock, res->ai_addr, res->ai_addrlen) != 0) {
		fprintf(stderr, "ERROR: bind failed.\n");
		goto error_addrinfo;
	}

	if (listen(sock, SOMAXCONN) != 0) {
		fprintf(stderr, "ERROR: listen failed.\n");
		goto error_addrinfo;
	}

	freeaddrinfo(res);

	addrlen = sizeof(addr);

	while ((client = accept(sock, (struct sockaddr*)&addr, &addrlen)) != -1) {

		ssl = SSL_new(ctx);
		SSL_set_fd(ssl, client);

		if (SSL_accept(ssl) != 1)
			fprintf(stderr, "ERROR: SSL_accept failed.\n");
		else
			while (1) {
				if (on_request(ssl)) break;
				
				sleep(1);
			}

		SSL_shutdown(ssl);
		SSL_free(ssl);
		closesocket(client);
	}

	closesocket(sock);
	SSL_CTX_free(ctx);
	return 0;

error_ctx:
	SSL_CTX_free(ctx);
	return -1;

error_addrinfo:
	SSL_CTX_free(ctx);
	freeaddrinfo(res);
	return -1;
}

#endif /*_CSOCKETS_H_*/
