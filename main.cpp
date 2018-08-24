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
	int servfd, sockfd, n, nfds, epollfd;
	unsigned int len,len_recv,num;
	unsigned long id;

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
	ev.data.fd = servfd;
	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, servfd, &ev) == -1) {
		logger.printf("epoll_ctl: servfd failed\n");
		exit(-1);
	}
	logger.printf("init success\n");

	for (;;) {
	   nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);
	   if (nfds == -1) {
			logger.printf("epoll_wait failed\n");
	    	exit(-1);
	   }
	   logger.printf("nfds %d\n",nfds);

	   for (n = 0; n < nfds; ++n) {
	       if (events[n].data.fd == servfd) {
	           sockfd = accept(servfd,NULL,NULL);
	           logger.printf("accept %d\n",sockfd);
	           if (sockfd == -1) {
					logger.printf("accept failed\n");
	        		exit(-1);
	           }
	           ev.events = EPOLLIN | EPOLLET;
	           ev.data.fd = sockfd;
	           ev.data.u32=0;//标记为未绑定
	           if (epoll_ctl(epollfd, EPOLL_CTL_ADD, sockfd,&ev) == -1) {
					logger.printf("epoll_ctl: sockfd failed\n");
					exit(-1);
	           }
	       } else {
	       		//handle data
	       		sockfd=events[n].data.fd;
	       		logger.printf("handle data %d\n",sockfd);
	       		if(recv(sockfd,&len,4,MSG_DONTWAIT|MSG_PEEK)!=4){
	       			logger.printf("wait more data\n");
	       			continue;//wait more data.
	       		}
	       		len=ntohl(len);
	       		if(len>MAX_MESSAGE_SIZE || len<5 || ioctl(sockfd,FIONREAD,&len_recv)==-1){
	       			logger.printf("invalid len or ioctl failed\n");
	       			if(events[n].data.u32==1){
	       				data.remove(events[n].data.u64);
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
	       			if(events[n].data.u32==1){
	       				data.remove(events[n].data.u64);
	       			}else{
	       				close(sockfd);
	       			}
	       			continue;
	       		}

	       		//process data
	       		if(buff[4]==1){//bind
	       			if(events[n].data.u32==1){//already bound
	       				data.remove(events[n].data.u64);
	       				continue;
	       			}
	       			if(len!=13){//invalid param
	       				logger.printf("invalid param\n");
	       				close(sockfd);
	       				continue;
	       			}
	       			id=ntohl64(buff+5);
	       			if(send(sockfd,ack_ok,7,MSG_NOSIGNAL)==-1){
	       				logger.printf("send ack failed\n");
	       				close(sockfd);
	       			}else{//bind success
	       				ev.events = EPOLLIN | EPOLLET;
						ev.data.fd = sockfd;
						ev.data.u32=1;//标记为已绑定
						ev.data.u64=id;
	       				if (epoll_ctl(epollfd, EPOLL_CTL_MOD, sockfd,&ev) == -1) {
							logger.printf("epoll_ctl: EPOLL_CTL_MOD failed\n");
							close(sockfd);
							continue;
			            }
	       				data.insert(id,(p=new client(id,sockfd)),false);
	       			}
	       		}else if(buff[4]==2){//push
	       			if(len<9){
	       				logger.printf("cmd 2 :len(%d)<9\n",len);
	       				if(events[n].data.u32==1){
		       				data.remove(events[n].data.u64);
		       			}else{
		       				close(sockfd);
		       			}
	       				continue;
	       			}
	       			memcpy(&num,buff+5,4);
	       			num=ntohl(num);
	       			if(len<(9+num*8)){
	       				logger.printf("cmd 2 :len(%d) <%d\n", len,9+num*8);
	       				if(events[n].data.u32==1){
		       				data.remove(events[n].data.u64);
		       			}else{
		       				close(sockfd);
		       			}
	       				continue;
	       			}
	       			const char *content=buff+9+num*8;
	       			len-=9+num*8;
	       			int len_send=htonl(len+4);
	       			for(int i=0;i<num;++i){
	       				id=ntohl64(buff+9+8*i);
	       				if(p=(client *)data.search(id,false)){
	       					if(send(p->fd,&len_send,4,MSG_NOSIGNAL)==-1 || send(p->fd,content,len,MSG_NOSIGNAL)==-1){
	       						data.remove(id);
	       						logger.printf("(id:%ld) push failed,broken link\n",id);
	       					}
	       				}
	       			}
	       		}else{//unknown cmd
	       			if(events[n].data.u32==1){
	       				data.remove(events[n].data.u64);
	       			}else{
	       				close(sockfd);
	       			}
	       		}
	       }
	   }
	}
}