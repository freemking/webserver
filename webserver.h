#ifndef _WEBSERVER_

#define _WEBSERVER_

#define HOSTLEN 256
#define BACKLOG 10

int connect_to_server(char *host,int portnum);
int make_server_socket(int portnum,int backlog);

#endif
