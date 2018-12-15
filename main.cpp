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

	if(argc<2){
		logger.printf("Usage: %s port\n", argv[0]);
		exit(-1);
	}

	uint32_t buff_size=0;
	if(argc>2){
		buff_size=atoi(argv[2]);
	}
	if(buff_size<=0){
		buff_size=10*1024;
	}
	uint32_t buff_size_big_endian = htonl(buff_size);


	std::unordered_map<int, int> data;
	std::unordered_map<int, int>::iterator search;
	char buff[buff_size];
	struct KeepConfig cfg = { 5, 2, 2};

	uint32_t max_events=10;
	struct epoll_event ev, events[max_events];
	uint32_t servfd, sockfd, n, nfds, epollfd;
	uint32_t len,len_2,num;
	uint64_t id;
	struct sockaddr addr;
    socklen_t addrlen;

	if((servfd=initTcpServer(argv[1]))==-1){
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
	logger.printf("init success\n");

	for (;;) {
	   nfds = epoll_wait(epollfd, events, max_events, -1);
	   if (nfds == -1) {
			logger.printf("epoll_wait failed\n");
	    	exit(-1);
	   }

	   for (n = 0; n < nfds; ++n) {
	   		sockfd=events[n].data.u64;
	       if (sockfd == servfd) {
	           sockfd = accept(servfd,&addr,&addrlen);
	           logger.printf("accept:%d\n",addrlen);
	           if (sockfd == -1) {
					logger.printf("accept failed\n");
	        		exit(-1);
	           }
	           set_tcp_keepalive_cfg(sockfd, &cfg);
	           ev.events = EPOLLIN;
	           ev.data.u64 = sockfd;
	           if (epoll_ctl(epollfd, EPOLL_CTL_ADD, sockfd,&ev) == -1) {
					logger.printf("epoll_ctl: sockfd failed\n");
					exit(-1);
	           }
	       } else {
	       		//handle data
	       		id=events[n].data.u64>>32;
	       		len_2=recv(sockfd,&len,4,MSG_DONTWAIT|MSG_PEEK);
	       		if(len_2<=0){
	       			if(id!=0){
	       				data.erase(id);
	       			}
	       			close(sockfd);
	       			logger.printf("broken link,clear\n");
	       			continue;
	       		}

	       		if(len_2!=4){ //wait more data.
	       			ev.events = EPOLLIN | EPOLLET;
					ev.data.u64=id<<32 | sockfd;
       				if (epoll_ctl(epollfd, EPOLL_CTL_MOD, sockfd,&ev) == -1){
		       			if(id!=0){
		       				data.erase(id);
		       			}
		       			close(sockfd);
		       			logger.printf("epoll_ctl: EPOLL_CTL_MOD failed\n");
       				}
	       			continue;
	       		}
	       		len=ntohl(len);
	       		if(len>buff_size || len<5 || ioctl(sockfd,FIONREAD,&len_2)==-1){
	       			logger.printf("invalid len or ioctl failed\n");
	       			if(id!=0){
	       				data.erase(id);
	       			}
	       			close(sockfd);
	       			continue;
	       		}
	       		if(len_2<len){//wait more data.
	       			ev.events = EPOLLIN | EPOLLET;
					ev.data.u64=id<<32 | sockfd;
       				if (epoll_ctl(epollfd, EPOLL_CTL_MOD, sockfd,&ev) == -1){
		       			if(id!=0){
		       				data.erase(id);
		       			}
		       			close(sockfd);
		       			logger.printf("epoll_ctl: EPOLL_CTL_MOD failed\n");
       				}
	       			continue;
	       		}
	       		if(recv(sockfd,buff,len,MSG_DONTWAIT)!=len){
	       			logger.printf("recv len != len\n");
	       			if(id!=0){
	       				data.erase(id);
	       			}
	       			close(sockfd);
	       			continue;
	       		}
	       		ev.events = EPOLLIN;
	       		ev.data.u64=id<<32 | sockfd;
	       		if (epoll_ctl(epollfd, EPOLL_CTL_MOD, sockfd,&ev) == -1){
       				if(id!=0){
	       				data.erase(id);
	       			}
	       			close(sockfd);
	       			logger.printf("epoll_ctl: EPOLL_CTL_MOD failed\n");
	       		}

	       		char cmd=buff[4];
	       		//process data
	       		if(cmd==0){//echo
	       			len_2 = htonl(len-1);
	       			if(send(sockfd,&len_2,4,MSG_NOSIGNAL)==-1 || send(sockfd,buff+5,len-5,MSG_NOSIGNAL)==-1){
	       				if(id!=0){
							data.erase(id);
	       				}
   						close(sockfd);
   						logger.printf("push echo failed,broken link\n",id);
	       			}
	       		}else if(cmd==1){//bind
	       			if(id!=0){//already bound
	       				search=data.find(id);
	       				if(search==data.end() || search->second!=sockfd){
	       					logger.printf("error prev!=sockfd\n");
	       				}
	       				data.erase(id);
	       				close(sockfd);
	       				continue;
	       			}
	       			if(len!=9){//invalid param
	       				logger.printf("invalid param len!=9\n");
	       				close(sockfd);
	       				continue;
	       			}
	       			memcpy(&id,buff+5,4);
	       			id=ntohl(id);
	       			if(id==0){
	       				logger.printf("invalid param, id=0\n");
	       				close(sockfd);
	       				continue;
	       			}
       				ev.events = EPOLLIN;
					ev.data.u64=id<<32 | sockfd;
       				if (epoll_ctl(epollfd, EPOLL_CTL_MOD, sockfd,&ev) == -1) {
						logger.printf("epoll_ctl: EPOLL_CTL_MOD failed\n");
						close(sockfd);
						continue;
		            }
		            search=data.find(id);
		            if(search!=data.end()){
		            	close(search->second);
		            }
		            data[id]=sockfd;
	       		}else if(cmd==2){//single push
	       			if(len<9){
	       				logger.printf("single push :len(%d)<9\n",len);
	       				if(id!=0){
		       				data.erase(id);
		       			}
		       			close(sockfd);
	       				continue;
	       			}
	       			memcpy(&id,buff+5,4);
	       			id=ntohl(id);
	       			search=data.find(id);
	       			if(search!=data.end()){
	       				sockfd=search->second;
	       				len_2 = htonl(len-5);
	       				if(send(sockfd,&len_2,4,MSG_NOSIGNAL)==-1 || send(sockfd,buff+9,len-9,MSG_NOSIGNAL)==-1){
	       					data.erase(id);
	       					close(sockfd);
	       					logger.printf("(id:%ld) push failed,broken link\n",id);
	       				}
	       			}
	       		} else if(cmd==3){//multi push
	       			if(len<9){
	       				logger.printf("multi push :len(%d)<9\n",len);
	       				if(id!=0){
		       				data.erase(id);
		       			}
		       			close(sockfd);
	       				continue;
	       			}
	       			memcpy(&num,buff+5,4);
	       			num=ntohl(num);
	       			if(len<(9+num*4)){
	       				logger.printf("multi push :len(%d) <%d\n", len,9+num*4);
	       				if(id!=0){
		       				data.erase(id);
		       			}
		       			close(sockfd);
	       				continue;
	       			}
	       			const char *content=buff+9+num*4;
	       			len-=9+num*4;
	       			int len_2=htonl(len+4);
	       			for(int i=0;i<num;++i){
	       				memcpy(&id,buff+9+4*i,4);
	       				id=ntohl(id);
	       				search=data.find(id);
	       				if(search!=data.end()){
	       					sockfd=search->second;
	       					if(send(sockfd,&len_2,4,MSG_NOSIGNAL)==-1 || send(sockfd,content,len,MSG_NOSIGNAL)==-1){
	       						data.erase(id);
	       						close(sockfd);
	       						logger.printf("(id:%ld) push failed,broken link\n",id);
	       						logger.printf("errno:%d\n", errno);
	       					}
	       				}
	       			}
	       		}else if(cmd==4){//get buffer size in bytes
	       			if(len!=5){
	       				if(id!=0){
							data.erase(id);
	       				}
   						close(sockfd);
	       				logger.printf("get buffer size invalid param, len(%d)!=5\n", len);
	       				continue;
	       			}
	       			len_2=htonl(8);
	       			if(send(sockfd,&len_2,4,MSG_NOSIGNAL)==-1 || send(sockfd,&buff_size_big_endian,4,MSG_NOSIGNAL)==-1){
	       				if(id!=0){
							data.erase(id);
	       				}
   						close(sockfd);
	       				logger.printf("push buffer size failed,broken link\n");
	       			}
	       		}else if(cmd==5){
	       			if(len!=5){
	       				if(id!=0){
							data.erase(id);
	       				}
   						close(sockfd);
	       				logger.printf("get buffer size invalid param, len(%d)!=5\n", len);
	       				continue;
	       			}
	       			len_2=htonl(4+addrlen);
	       			if(send(sockfd,&len_2,4,MSG_NOSIGNAL)==-1 || send(sockfd,&addr,addrlen,MSG_NOSIGNAL)==-1){
	       				if(id!=0){
							data.erase(id);
	       				}
   						close(sockfd);
	       				logger.printf("push buffer size failed,broken link\n");
	       			}
	       		} else{//unknown cmd
	       			logger.printf("unknow cmd: %d\n", cmd);
	       			if(id!=0){
	       				data.erase(id);
	       			}
	       			close(sockfd);
	       		}
	       }
	   }
	}
}