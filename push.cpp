#include "push.h"

//============================Log===================================
Log::Log(const char *path){
	char *pathname=new char[strlen(path)+5]();
	strcpy(pathname,path);
	file=fopen(strcat(pathname,"/log"),"a");
	delete pathname;
}

Log::~Log(){fclose(file);}

void Log::printf(const char *format,...){
	time(&t);
	tmp=localtime(&t);
	strftime(buff,20,"%F %T",tmp);
	fprintf(file,"%s ",buff);
	va_list args;
	va_start(args,format);
	vfprintf(file, format, args);
	va_end(args);
	
	fflush(file);
}

void Log::flush(){
	fflush(file);
}


//============================function===================================
long getCurrentTime(){
   struct timeval tv;
   gettimeofday(&tv,NULL);
   return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

int set_tcp_keepalive(int sockfd){
    int optval = 1;
    return setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval));
}

int set_tcp_keepalive_cfg(int sockfd, const struct KeepConfig *cfg){
    int rc;

    //first turn on keepalive
    rc = set_tcp_keepalive(sockfd);
    if (rc != 0) {
        return rc;
    }

    //set the keepalive options
    rc = setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPCNT, &cfg->keepcnt, sizeof cfg->keepcnt);
    if (rc != 0) {
        return rc;
    }

    rc = setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPIDLE, &cfg->keepidle, sizeof cfg->keepidle);
    if (rc != 0) {
        return rc;
    }

    rc = setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPINTVL, &cfg->keepintvl, sizeof cfg->keepintvl);
    if (rc != 0) {
        return rc;
    }
    return 0;
}

int initTcpServer(const char * port){

	struct addrinfo *aip;
	struct addrinfo hint;
	
	int fd;
	
	memset(&hint,0,sizeof(hint));
	hint.ai_flags=AI_PASSIVE;
	hint.ai_socktype=SOCK_STREAM;
	hint.ai_canonname=NULL;
	hint.ai_addr=NULL;
	hint.ai_next=NULL;

	if((getaddrinfo(NULL,port,&hint,&aip))!=0){
		return -1;
	}

	if((fd=socket(aip->ai_addr->sa_family,SOCK_STREAM,0))<0){
		return -1;
	}
	if((bind(fd,aip->ai_addr,aip->ai_addrlen))<0){
		close(fd);
		return -1;
	}
	if((listen(fd,128))<0){
		close(fd);
		return -1;
	}
	return fd;
}


int initTcpClient(const char *ip,const char *port){
	struct addrinfo *aip;
	struct addrinfo hint;
	
	memset(&hint,0,sizeof(hint));
	hint.ai_flags=0;
	hint.ai_socktype=SOCK_STREAM;
	hint.ai_canonname=NULL;
	hint.ai_addr=NULL;
	hint.ai_next=NULL;
	if(getaddrinfo(ip,port,&hint,&aip)!=0){
		return -1;
	}

	int fd;
	if((fd=socket(AF_INET,SOCK_STREAM,0))<0)
		return -1;
	if(connect(fd,aip->ai_addr,aip->ai_addrlen)==0)
		return fd;
	close(fd);
	return -1;
}

int daemonize(const char *cmd){
	int i,fd0,fd1,fd2;
	pid_t pid;
	struct rlimit rl;
	struct sigaction sa;

	//clear file creation mask.
	umask(0);

	//get maximum number of file descriptors.
	if(getrlimit(RLIMIT_NOFILE,&rl)<0)
		return -1;

	//become a session leader to lose controlling TTY.
	if((pid=fork())<0)
		return -1;
	else if(pid!=0) //parent
		exit(0);
	setsid();

	//ensure future opens won't allocate controlling TTYs.

	sa.sa_handler=SIG_IGN;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags=0;
	if(sigaction(SIGHUP,&sa,NULL)<0)
		return -1;
	if((pid=fork())<0)
		return -1;
	else if(pid!=0) //parent
		exit(0);

	//change the current working directory to the root so
	//we won't prevent file systems from being unmounted.
	// if(chdir("/")<0)
	// 	return -1;

	//close all open file descriptors.
	if(rl.rlim_max==RLIM_INFINITY)
		rl.rlim_max=1024;
	for(i=0;i<rl.rlim_max;i++)
		close(i);

	//attach file descriptors 0,1,and 2 to /dev/null.
	fd0=open("/dev/null",O_RDWR);
	fd1=dup(0);
	fd2=dup(0);

	//initialize the log file.
	openlog(cmd,LOG_CONS,LOG_USER);
	if(fd0!=0 || fd1!=1 || fd2!=2){
		syslog(LOG_ERR,"unexpected file descriptors %d %d %d",fd0,fd1,fd2);
		exit(1);
	}
}