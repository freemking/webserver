#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <time.h>

#include "webserver.h"
time_t server_started;
int    server_bytes_sent,sin_size,server_requests;
struct sockaddr_in remote_addr;

int main(int argc, char *argv[])
{
	int sock ,fd;
	int *fdptr;
	struct timeval tv;
	unsigned long start_time;
	pthread_t worker;
	pthread_attr_t attr;

	void *handle_call(void *);

	if(argc == 1){
		fprintf(stderr,"usage;tws portnum");
		exit(1);
	}

	sock = make_server_socket(atoi(argv[1]),BACKLOG);
	if(sock == -1){
		perror("making socket");
		exit(2);
	}

	setup(&attr);

	while(1){
		sin_size = sizeof(struct sockaddr_in);
		fd = accept(sock,(struct sockaddr*)&remote_addr,&sin_size);
		gettimeofday(&tv,(struct timezone *)NULL);
		start_time = (tv.tv_sec * 1000000)+tv.tv_usec;
		server_requests++;
		printf("total = %d\n",server_requests);
		fdptr = (int *)malloc(sizeof(int));
		*fdptr = fd;
		pthread_create(&worker,&attr,handle_call,fdptr);
		gettimeofday(&tv,(struct timezone *)NULL);
		printf("snprintf time: %ld\n", (tv.tv_sec * 1000000) + tv.tv_usec - start_time);
	}
	return 0;
}

/*
 * initialize the status var lables and
 *  * set the thread attribute to detached
 *   */

setup(pthread_attr_t *attrp)
{
	pthread_attr_init(attrp);
	pthread_attr_setdetachstate(attrp,PTHREAD_CREATE_DETACHED);

	time(&server_started);
	server_requests = 0;
	server_bytes_sent = 0;
}

void *handle_call(void *fdptr)
{
	FILE *fpin;
	char request[BUFSIZ];
	int fd;

	fd = *(int *)fdptr;
	free(fdptr);

	fpin = fdopen(fd,"r");
	fgets(request,BUFSIZ,fpin);
	printf("got a call on %d;request = %s", fd, request);
	process_rq(request,fd);
	fclose(fpin);
}

skip_rest_of_header(FILE *fp)
{
	char buf[BUFSIZ] ;
	while(fgets(buf,BUFSIZ,fp) != NULL && strcmp(buf,"\r\n") != 0)
		;
}

access_log(char *request){
	FILE *logfp;
	logfp = fopen("access.log","a");
	fputs(request,logfp);
	fclose(logfp);
}

process_rq(char *rq,int fd)
 {
	char cmd[BUFSIZ],arg[BUFSIZ];
	
	access_log(rq);

	if(sscanf(rq,"%s %s,",cmd,arg) != 2)
		return ;
	sanitize(arg);
	printf("sanitized version is %s\n",arg);

	if(strcmp(cmd,"GET") != 0){
		not_implemented();
	}
	else if(bulit_in(arg,fd)){
		;
	}
	else if(not_exist(arg)){
		do_404(arg,fd);
	}
	else if(isadir(arg)){
		do_ls(arg,fd);
	}
	else{
		do_cat(arg,fd);
	}
	
 }

 sanitize(char *str)
 {
	char *src,*dest;
	src = dest = str;
	while(*src){
		if(strncmp(src,"/../",4) == 0)
			src += 3;
		else if(strncmp(src,"//",2) == 0)
			src++;
		else
			*dest++ = *src++;
	}

	*dest = '\0';
	if(*str == '/')
		strcpy(str,str+1);
	if(str[0] == '\0' || strcmp(str,"./") == 0 || strcmp(str,"./..") == 0)
		strcpy(str,".");
 }

 bulit_in(char *arg,int fd)
 {
	FILE *fp;
	if(strcmp(arg,"status") != 0)
		return 0;
	http_reply(fd,&fp,200,"OK","text/plain",NULL);
	fprintf(fp,"Server started: %s",ctime(&server_started));
	fprintf(fp,"Total requests: %d\n",server_requests);
	fprintf(fp,"Bytes sent out:%d\n",server_bytes_sent);
	fclose(fp);
	return 1;
 }

 http_reply(int fd,FILE **fpp,int code,char *msg,char *type,char *content)
 {
	FILE *fp = fdopen(fd,"w");
	int bytes = 0;
	if(fp != NULL){
		bytes = fprintf(fp,"HTTP/1.0 %d %s \r\n",code,msg);
		bytes += fprintf(fp,"Content-type: %s\r\n\r\n",type);
		if(content)
			bytes += fprintf(fp,"%s\r\n",content);
	}
	fflush(fp);
	if(fpp)
		*fpp = fp;
	else
		fclose(fp);
	return bytes;
 }

 not_implemented(int fd)
 {
	http_reply(fd,NULL,501,"Not Implemented","text/plain","The item you seek is not here");
 }

 do_404( char *item,int fd)
 {
	http_reply(fd,NULL,404,"Not Found","text/plain","The item you seek is not here");
 }

 isadir(char *f){
	struct stat info;
	return (stat(f,&info) != -1 && S_ISDIR(info.st_mode));
 }

 not_exist(char *f){
	struct stat info;
	return (stat(f,&info) == -1);
 }

 do_ls(char *dir,int fd)
 {
	DIR *dirptr;
	struct dirent *direntp;
	FILE *fp;
	int bytes = 0;
	bytes = http_reply(fd,&fp,200,"OK","text/plain",NULL);
	bytes = fprintf(fp,"Listing of Directory %s \n",dir);

	if((dirptr = opendir(dir)) != NULL){
		while(direntp = readdir(dirptr)){
			bytes += fprintf(fp,"%s\n",direntp->d_name);
		}
		closedir(dirptr);
	}
	fclose(fp);
	server_bytes_sent += bytes;
 }

 char * file_type(char *f)
 {
	char *cp;
	if((cp = strrchr(f,'.')) != NULL)
		return cp+1;
	return "  -";
 }

 do_cat(char *f,int fd)
 {
	char *extension = file_type(f);
	char *type = "text/plain";
	FILE *fpsock,*fpfile;
	int c;
	int bytes = 0;
	if(strcmp(extension,"html") == 0)
		type = "text/html";
	else if(strcmp(extension,"gif") == 0)
		type = "image/gif";
	else if(strcmp(extension,"jpg") == 0)
		type = "image/jpeg";
	else if(strcmp(extension,"jpeg") == 0)
		type = "image/jpeg";
	fpsock = fdopen(fd,"w");
	fpfile = fopen(f,"r");
	if(fpsock != NULL && fpfile != NULL){
		bytes = http_reply(fd,&fpsock,200,"OK",type,NULL);
		while((c = getc(fpfile)) != EOF){
			putc(c,fpsock);
			bytes ++;
		}
		fclose(fpfile);
		fclose(fpsock);
	}
	server_bytes_sent += bytes;
 }

int connect_to_server(char *host,int portnum)
{
	int sock;
	struct sockaddr_in servadd;
	struct hostent *hp;
	sock = socket(AF_INET,SOCK_STREAM,0);
	if(sock == -1)
		return -1;
	bzero(&servadd,sizeof(servadd));
	hp = gethostbyname(host);
	if(hp == NULL)
		return -1;
	bcopy((void *)hp->h_addr,(void *)&servadd.sin_addr,hp->h_length);
	servadd.sin_port = htons(portnum);
	servadd.sin_family = AF_INET;
	if(connect(sock,(struct sockaddr *)&servadd,sizeof(servadd)) != 0)
		return -1;
	return sock;
}

int make_server_socket(int portnum,int backlog)
{
	struct sockaddr_in saddr;
	struct hostent *hp;
	char hostname[HOSTLEN];
	int sock_id;

	sock_id = socket(PF_INET,SOCK_STREAM,0);
	if(sock_id == -1)
		return -1;

	bzero((void *)&saddr,sizeof(saddr));
	gethostname(hostname,HOSTLEN);
	hp = gethostbyname(hostname);

	bcopy((void *)hp->h_addr,(void *)&saddr.sin_addr,hp->h_length);
	saddr.sin_port = htons(portnum);
	saddr.sin_family = AF_INET;
	saddr.sin_addr.s_addr = INADDR_ANY;
	if(bind(sock_id,(struct sockaddr *)&saddr,sizeof(saddr)) != 0)
		return -1;
	if(listen(sock_id,backlog) != 0)
		return -1;
	return sock_id;
}

