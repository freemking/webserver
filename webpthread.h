#ifndef _WEBPTHREAD_

#define _WEBPTHREAD_

#define HOSTLEN 256
#define BACKLOG 10

int connect_to_server(char *host,int portnum);
int make_server_socket(int portnum,int backlog);

#endif
