/* socket functions */

/* on -1 return the functions will have printed the
   error messages themselves */

/* open socket
   returns -1 on error, socket descriptor on success */
int sock_open(void);

/* connect socket to host (by name) on specified port
   returns -1 on error, 0 on success */
int sock_connect(int sock, const char *host, int port);

/* send data through socket (buffer is 0-terminated) */
int sock_send(int sock, const char *buffer);

/* read data from socket; buffer will be 0-terminated; bufsize
   is width of buffer (incl. 0 byte); timeout is in seconds
   returns -1 on error, number of bytes read (excl. 0 byte) on
   sucess (0 means timeout) */
int sock_recv(int sock, char *buffer, int bufsize, int timeout);

/* reads line (terminated with CR/LF) from socket
   same behaviour as sock_recv */
int sock_recvline(int sock, char *buffer, int bufsize, int timeout);

/* close socket */
void sock_close(int sock);
