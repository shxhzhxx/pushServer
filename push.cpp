#include "push.h"

//============================logger===================================
logger::logger(const char *path){
	file=fopen(path,"a");
}

logger::~logger(){fclose(file);}

void logger::printf(const char *format,...){
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

void logger::flush(){
	fflush(file);
}

//============================message===================================
message::message(const char *_content,int _callback_type,long _id,int _push_id,const char *_callback_url):push_id(_push_id),callback_url(0),callback_type(_callback_type),id(_id),push_success(false),next(0),timestamp(0),p(0),prev(0),callback_fd(0){
	initial(_content,_callback_url);
}

message::message(const char *_content,int _callback_type,user_data *_p,int _push_id,const char *_callback_url):push_id(_push_id),callback_url(0),callback_type(_callback_type),push_success(false),next(0),timestamp(0),p(_p),prev(0),callback_fd(0){
	initial(_content,_callback_url);
	id=_p->id;
}

message::~message(){
	delete []content;
	delete []callback_url;
	delete next;
}

void message::initial(const char *_content,const char *_callback_url){
	content=new char[strlen(_content)+1]();
	strcpy(content,_content);
	if(_callback_url){
		int index=0;
		for (int i = 0; _callback_url[i]!=0; ++i){
			if(_callback_url[i]!='\\'){
				++index;
			}
		}
		callback_url=new char[index+1]();
		index=0;
		for (int i = 0; _callback_url[i]!=0; ++i){
			if(_callback_url[i]!='\\'){
				callback_url[index]=_callback_url[i];
				++index;
			}
		}
	}
}

//============================user_data===================================
user_data::user_data(long _id,int _fd) :id(_id), fd(_fd),msg_f(0),msg_l(0) {
	msg_mutex = new pthread_mutex_t();
	pthread_mutex_init(msg_mutex,NULL);
}

user_data::~user_data() {
	delete msg_f;
	close(fd);
	pthread_mutex_destroy(msg_mutex);
    delete msg_mutex;
}

void user_data::append_message(message *msg){
	pthread_mutex_lock(msg_mutex);
	if(msg_l){
		msg_l->next=msg;
		msg_l=msg_l->next;
	}else{
		msg_f=msg;
		msg_l=msg;
	}
	pthread_mutex_unlock(msg_mutex);
}

message *user_data::pop_message(){
	pthread_mutex_lock(msg_mutex);
	message *ret=0;
	if(msg_f){
		ret=msg_f;
		if(msg_f==msg_l){
			msg_l=0;
		}
		msg_f=msg_f->next;
		ret->next=0;
	}
	pthread_mutex_unlock(msg_mutex);
	return ret;
}


//============================link_list===================================
link_list::link_list(): msg_f(0),msg_l(0) {
	mutex = new pthread_mutex_t();
	pthread_mutex_init(mutex,NULL);
	cond = new pthread_cond_t();
	pthread_cond_init(cond,NULL);
}

link_list::~link_list(){
	pthread_mutex_destroy(mutex);
    delete mutex;
    pthread_cond_destroy(cond);
    delete cond;
}

void link_list::append_message_to_head(message *msg){
	pthread_mutex_lock(mutex);
	msg->next=msg_f;
	if(msg_f){
		msg_f->prev=msg;
	}
	msg_f=msg;
	pthread_cond_signal(cond);
	pthread_mutex_unlock(mutex);
}

void link_list::append_message(message *msg){
	pthread_mutex_lock(mutex);
	msg->timestamp=getCurrentTime();
	if(msg_l){
		msg_l->next=msg;
		msg->prev=msg_l;
		msg_l=msg_l->next;
	}else{
		msg_f=msg;
		msg_l=msg;
	}
	pthread_cond_signal(cond);
	pthread_mutex_unlock(mutex);
}

message *link_list::pop_message(){
	pthread_mutex_lock(mutex);
	message *ret=0;
	if(msg_f){
		ret=msg_f;
		if(msg_f==msg_l){
			msg_l=0;
		}
		msg_f=msg_f->next;
		if(msg_f){
			msg_f->prev=0;
		}
		ret->next=0;
	}
	pthread_mutex_unlock(mutex);
	return ret;
}

message *link_list::pop_message_wait(){
	pthread_mutex_lock(mutex);
	message *ret=0;
	while(!msg_f){
		pthread_cond_wait(cond,mutex);
	}
	ret=msg_f;
	if(msg_f==msg_l){
		msg_l=0;
	}
	msg_f=msg_f->next;
	if(msg_f){
		msg_f->prev=0;
	}
	ret->next=0;
	pthread_mutex_unlock(mutex);
	return ret;
}

message *link_list::take_out_message(long id){
	pthread_mutex_lock(mutex);
	message *p=msg_f;
	while(p){
		if(p->p && p->p->id==id){
			if(p->prev){
				p->prev->next=p->next;
			}else{
				msg_f=p->next;
			}
			if(p->next){
				p->next->prev=p->prev;
			}else{
				msg_l=p->prev;
			}
			p->next=0;
			pthread_mutex_unlock(mutex);
			return p;
		}
		p=p->next;
	}
	pthread_mutex_unlock(mutex);
	return NULL;
}

message *link_list::take_out_message(message *msg){
	pthread_mutex_lock(mutex);
	message *p=msg_f;
	while(p){
		if(p==msg){
			if(p->prev){
				p->prev->next=p->next;
			}else{
				msg_f=p->next;
			}
			if(p->next){
				p->next->prev=p->prev;
			}else{
				msg_l=p->prev;
			}
			p->next=0;
			pthread_mutex_unlock(mutex);
			return p;
		}
		p=p->next;
	}
	pthread_mutex_unlock(mutex);
	return NULL;
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
	hint.ai_flags=AI_CANONNAME;
	hint.ai_socktype=SOCK_STREAM;
	hint.ai_canonname=NULL;
	hint.ai_addr=NULL;
	hint.ai_next=NULL;

	if((getaddrinfo(SERVER_IP_ADDRESS,port,&hint,&aip))!=0){
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
	hint.ai_flags=AI_CANONNAME;
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
	if(chdir("/")<0)
		return -1;

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