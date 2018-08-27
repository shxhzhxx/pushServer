#include "push.h"
#include <sys/ioctl.h>
#include <string.h>
#include <arpa/inet.h>


int main(int argc,char *argv[]){
	daemonize("push_server");

	char *path=getcwd(NULL,0);
	if(path==NULL){
		exit(-1);
	}
	Log logger(path);
	delete path;

	rb_tree data;
	client *p=0;
	char buff[MAX_MESSAGE_SIZE];
	char ack_ok[7]={0,0,0,7,'2','0','0'};

	struct epoll_event ev, events[MAX_EVENTS];
	uint32_t servfd, sockfd, n, nfds, epollfd;
	uint32_t len,len_recv,num;
	uint64_t id;

	if((servfd=initTcpServer(SERVER_PORT))==-1){
		logger.printf("initTcpServer failed\n");
		exit(-1);
	}

	epollfd = epoll_create1(0);
	if (epollfd == -1) {
		logger.printf("epoll_create1 failed\n");
		exit(-1);
	}

	ev.events = EPOLLIN;
	ev.data.u64=servfd;
	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, servfd, &ev) == -1) {
		logger.printf("epoll_ctl: servfd failed\n");
		exit(-1);
	}
	logger.printf("init success %d\n",servfd);

	for (;;) {
	   nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);
	   if (nfds == -1) {
			logger.printf("epoll_wait failed\n");
	    	exit(-1);
	   }
	   logger.printf("nfds %d\n",nfds);

	   for (n = 0; n < nfds; ++n) {
	   		sockfd=events[n].data.u64;
	       if (sockfd == servfd) {
	           sockfd = accept(servfd,NULL,NULL);
	           logger.printf("accept %d\n",sockfd);
	           if (sockfd == -1) {
					logger.printf("accept failed\n");
	        		exit(-1);
	           }
	           ev.events = EPOLLIN | EPOLLET;
	           ev.data.u64 = sockfd;
	           if (epoll_ctl(epollfd, EPOLL_CTL_ADD, sockfd,&ev) == -1) {
					logger.printf("epoll_ctl: sockfd failed\n");
					exit(-1);
	           }
	       } else {
	       		//handle data
	       		id=events[n].data.u64>>32;
	       		logger.printf("handle data %d\n",id);
	       		if(recv(sockfd,&len,4,MSG_DONTWAIT|MSG_PEEK)!=4){
	       			logger.printf("wait more data\n");
	       			continue;//wait more data.
	       		}
	       		len=ntohl(len);
	       		if(len>MAX_MESSAGE_SIZE || len<5 || ioctl(sockfd,FIONREAD,&len_recv)==-1){
	       			logger.printf("invalid len or ioctl failed\n");
	       			if(id!=0){
	       				data.remove(id);
	       			}else{
	       				close(sockfd);
	       			}
	       			continue;
	       		}
	       		if(len_recv<len){
	       			logger.printf("wait more data 2\n");
	       			continue;//wait more data.
	       		}
	       		if(recv(sockfd,buff,len,MSG_DONTWAIT)!=len){
	       			logger.printf("recv len != len\n");
	       			if(id!=0){
	       				data.remove(id);
	       			}else{
	       				close(sockfd);
	       			}
	       			continue;
	       		}

	       		//process data
	       		if(buff[4]==1){//bind
	       			logger.printf("bind %d\n",id);
	       			if(id!=0){//already bound
	       				data.remove(id);
	       				continue;
	       			}
	       			if(len!=9){//invalid param
	       				logger.printf("invalid param\n");
	       				close(sockfd);
	       				continue;
	       			}
	       			memcpy(&id,buff+5,4);
	       			id=ntohl(id);
	       			if(send(sockfd,ack_ok,7,MSG_NOSIGNAL)==-1){
	       				logger.printf("send ack failed\n");
	       				close(sockfd);
	       			}else{//bind success
	     //   				ev.events = EPOLLIN | EPOLLET;
						// ev.data.u64=id<<32 | sockfd;
	     //   				if (epoll_ctl(epollfd, EPOLL_CTL_MOD, sockfd,&ev) == -1) {
						// 	logger.printf("epoll_ctl: EPOLL_CTL_MOD failed\n");
						// 	close(sockfd);
						// 	continue;
			   //          }
	       				data.insert(id,(p=new client(id,sockfd)),false);
	       				logger.printf("bind success %d\n",id);
	       			}
	       		}else if(buff[4]==2){//push
	       			logger.printf("push %d\n",id);
	       			if(len<9){
	       				logger.printf("cmd 2 :len(%d)<9\n",len);
	       				if(id!=0){
		       				data.remove(id);
		       			}else{
		       				close(sockfd);
		       			}
	       				continue;
	       			}
	       			memcpy(&num,buff+5,4);
	       			num=ntohl(num);
	       			if(len<(9+num*4)){
	       				logger.printf("cmd 2 :len(%d) <%d\n", len,9+num*4);
	       				if(id!=0){
		       				data.remove(id);
		       			}else{
		       				close(sockfd);
		       			}
	       				continue;
	       			}
	       			const char *content=buff+9+num*4;
	       			len-=9+num*4;
	       			int len_send=htonl(len+4);
	       			for(int i=0;i<num;++i){
	       				memcpy(&id,buff+9+4*i,4);
	       				id=ntohl(id);
	       				if(p=(client *)data.search(id,false)){
	       					if(send(p->fd,&len_send,4,MSG_NOSIGNAL)==-1 || send(p->fd,content,len,MSG_NOSIGNAL)==-1){
	       						data.remove(id);
	       						logger.printf("(id:%ld) push failed,broken link\n",id);
	       					}
	       				}
	       			}
	       		}else{//unknown cmd
	       			if(id!=0){
	       				data.remove(id);
	       			}else{
	       				close(sockfd);
	       			}
	       		}
	       }
	   }
	}
}