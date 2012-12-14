#ifndef _WEBEPOLL_

#define _WEBEPOLL_

#define SERVER_PORT 8888
#define BUFFER_SIZE 400
#define MAX_EVENTS 10

void analyze_request(char *request,int fd);
void http_reply(int fd,FILE *fcontent,int code,char *msg,char *type,char *content);

void not_implemented(int fd);
int not_exist(char *f);
void do_404(char *f,int fd);
int is_dir(char *f);
void do_ls(char *dir,int fd); 
void do_cat(char *f,int fd);
char * file_type(char *f);

#endif
