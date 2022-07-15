# CSocket
A single-file HTTP/HTTPS client/server written in C with OpenSSL.

# HTTP Client Usage

### csocket_connect(host, port);
Connect to host:port. (usually port 80 for http)  
Returns a socket file descriptor.

### csocket_read(sock, buffer, buffer_size);
Reads from socket into a buffer of buffer_size.  
Returns number of bytes readed.

### csocket_write(sock, buffer, buffer_size);
Writes to the socket from a buffer of buffer_size.  
Returns number of bytes written.

### csocket_close(sock);
Closes the socket, finishing the connection.
Returns nothing.

# HTTPS Client Usage

### csocket_secure_connect(host, port);
Connect to host:port. (usually port 443 for https)  
Returns a SSL pointer.

### csocket_secure_read(ssl, buffer, buffer_size);
Reads from socket of SSL pointer, into a buffer of buffer_size.  
Returns number of bytes readed.

### csocket_secure_write(ssl, buffer, buffer_size);
Writes to the socket of SSL pointer, from a buffer of buffer_size.  
Returns number of bytes written.

### csocket_secure_close(ssl);
Frees the SSL pointer, finishing the connection.
Returns nothing.

# HTTP Server Usage

### csocket_listen(host, port, on_request);
Creates a server that listens on host:port. (usually localhost as host)  
The on_request argument needs to be filled with a callback function, which will be called on each request, passing the client socket as the argument, and returning true to close and finishing the connection, or false, to keep connected.

# HTTPS Server Usage

### csocket_secure_listen(host, port, on_request, cert_path, key_path);
Creates a server that listens on host:port. (usually localhost as host)  
The on_request argument needs to be filled with a callback function, which will be called on each request, passing the client socket as the argument, and returning true to close and finishing the connection, or false, to keep connected.
...
