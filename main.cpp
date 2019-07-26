#include "push.h"
#include <sys/ioctl.h>
#include <string.h>
#include <arpa/inet.h>


int main(int argc,char *argv[]){
	if(argc<2){
		printf("Need to specify a port to start the process, use\n\n    %s port\n\n", argv[0]);
		exit(-1);
	}

	daemonize("push_server");
	char *path=getcwd(NULL,0);
	if(path==NULL){
		exit(-1);
	}
	Log logger(path);
	delete path;

	uint32_t buff_size=0;
	if(argc>2){
		buff_size=atoi(argv[2]);
	}
	if(buff_size<=0){
		buff_size=10*1024;
	}
	uint32_t buff_size_big_endian = htonl(buff_size);


	std::unordered_set<int> unbound_clients;
	std::unordered_map<int, int> bound_clients;
	std::unordered_map<int, int>::iterator search;
	char buff[buff_size];
	struct KeepConfig cfg = { 5, 2, 2};

	uint32_t max_events=10;
	struct epoll_event ev, events[max_events];
	uint32_t servfd, sockfd, n, nfds, epollfd;
	uint32_t len,len_2,num;
	uint64_t id;
	struct sockaddr_in addr;
    socklen_t addrlen;
    char address[INET_ADDRSTRLEN];

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

	auto closeClient = [&bound_clients,&unbound_clients](int id,int sockfd){
		if(id!=0){
			bound_clients.erase(id);
		}else{
			unbound_clients.erase(sockfd);
		}
		close(sockfd);
	};

	for (;;) {
	   nfds = epoll_wait(epollfd, events, max_events, -1);
	   if (nfds == -1) {
			logger.printf("epoll_wait failed\n");
	    	exit(-1);
	   }

	   for (n = 0; n < nfds; ++n) {
	   		sockfd=events[n].data.u64;
	       if (sockfd == servfd) {
	           sockfd = accept(servfd,NULL,NULL);
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
	           }else{
	           		unbound_clients.emplace(sockfd);
	           }
	       } else {
	       		//handle data
	       		id=events[n].data.u64>>32;
	       		len_2=recv(sockfd,&len,4,MSG_DONTWAIT|MSG_PEEK);
	       		if(len_2<=0){
	       			closeClient(id,sockfd);
	       			logger.printf("broken link,clear\n");
	       			continue;
	       		}

	       		if(len_2!=4){ //wait more data.
	       			ev.events = EPOLLIN | EPOLLET;
					ev.data.u64=id<<32 | sockfd;
       				if (epoll_ctl(epollfd, EPOLL_CTL_MOD, sockfd,&ev) == -1){
		       			closeClient(id,sockfd);
		       			logger.printf("epoll_ctl: EPOLL_CTL_MOD failed\n");
       				}
	       			continue;
	       		}
	       		len=ntohl(len);
	       		if(len>buff_size || len<5 || ioctl(sockfd,FIONREAD,&len_2)==-1){
	       			closeClient(id,sockfd);
	       			logger.printf("invalid len or ioctl failed\n");
	       			continue;
	       		}
	       		if(len_2<len){//wait more data.
	       			ev.events = EPOLLIN | EPOLLET;
					ev.data.u64=id<<32 | sockfd;
       				if (epoll_ctl(epollfd, EPOLL_CTL_MOD, sockfd,&ev) == -1){
       					closeClient(id,sockfd);
		       			logger.printf("epoll_ctl: EPOLL_CTL_MOD failed\n");
       				}
	       			continue;
	       		}
	       		if(recv(sockfd,buff,len,MSG_DONTWAIT)!=len){
	       			closeClient(id,sockfd);
	       			logger.printf("recv len != len\n");
	       			continue;
	       		}
	       		ev.events = EPOLLIN;
	       		ev.data.u64=id<<32 | sockfd;
	       		if (epoll_ctl(epollfd, EPOLL_CTL_MOD, sockfd,&ev) == -1){
	       			closeClient(id,sockfd);
	       			logger.printf("epoll_ctl: EPOLL_CTL_MOD failed\n");
	       		}

	       		char cmd=buff[4];
	       		//process data
	       		if(cmd==0){//echo
	       			len_2 = htonl(len-1);
	       			if(send(sockfd,&len_2,4,MSG_NOSIGNAL)==-1 || send(sockfd,buff+5,len-5,MSG_NOSIGNAL)==-1){
	       				closeClient(id,sockfd);
   						logger.printf("push echo failed,broken link\n",id);
	       			}
	       		}else if(cmd==1){//bind
	       			if(id!=0){//already bound
	       				search=bound_clients.find(id);
	       				if(search==bound_clients.end() || search->second!=sockfd){
	       					logger.printf("error prev!=sockfd\n");
	       				}
	       				bound_clients.erase(id);
	       				close(sockfd);
	       				continue;
	       			}
	       			unbound_clients.erase(sockfd);//bind success or failed, we both need to erase it from unbound_clients.
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
		            search=bound_clients.find(id);
		            if(search!=bound_clients.end()){
		            	close(search->second);
		            }
		            bound_clients[id]=sockfd;
	       		}else if(cmd==2){//single push
	       			if(len<9){
	       				closeClient(id,sockfd);
	       				logger.printf("single push :len(%d)<9\n",len);
	       				continue;
	       			}
	       			memcpy(&id,buff+5,4);
	       			id=ntohl(id);
	       			search=bound_clients.find(id);
	       			if(search!=bound_clients.end()){
	       				sockfd=search->second;
	       				len_2 = htonl(len-5);
	       				if(send(sockfd,&len_2,4,MSG_NOSIGNAL)==-1 || send(sockfd,buff+9,len-9,MSG_NOSIGNAL)==-1){
	       					bound_clients.erase(id);
	       					close(sockfd);
	       					logger.printf("(id:%ld) push failed,broken link\n",id);
	       					logger.printf("errno:%d\n", errno);
	       				}
	       			}
	       		} else if(cmd==3){//multi push
	       			if(len<9){
	       				closeClient(id,sockfd);
	       				logger.printf("multi push :len(%d)<9\n",len);
	       				continue;
	       			}
	       			memcpy(&num,buff+5,4);
	       			num=ntohl(num);
	       			if(len<(9+num*4)){
	       				closeClient(id,sockfd);
	       				logger.printf("multi push :len(%d) <%d\n", len,9+num*4);
	       				continue;
	       			}
	       			const char *content=buff+9+num*4;
	       			len-=9+num*4;
	       			len_2=htonl(len+4);
	       			for(int i=0;i<num;++i){
	       				memcpy(&id,buff+9+4*i,4);
	       				id=ntohl(id);
	       				search=bound_clients.find(id);
	       				if(search!=bound_clients.end()){
	       					sockfd=search->second;
	       					if(send(sockfd,&len_2,4,MSG_NOSIGNAL)==-1 || send(sockfd,content,len,MSG_NOSIGNAL)==-1){
	       						bound_clients.erase(id);
	       						close(sockfd);
	       						logger.printf("(id:%ld) push failed,broken link\n",id);
	       						logger.printf("errno:%d\n", errno);
	       					}
	       				}
	       			}
	       		}else if(cmd==4){//get buffer size in bytes
	       			if(len!=5){
	       				closeClient(id,sockfd);
	       				logger.printf("get buffer size invalid param, len(%d)!=5\n", len);
	       				continue;
	       			}
	       			len_2=htonl(8);
	       			if(send(sockfd,&len_2,4,MSG_NOSIGNAL)==-1 || send(sockfd,&buff_size_big_endian,4,MSG_NOSIGNAL)==-1){
	       				closeClient(id,sockfd);
	       				logger.printf("push buffer size failed,broken link\n");
	       			}
	       		}else if(cmd==5){//get ip
	       			if(len!=5){
	       				closeClient(id,sockfd);
	       				logger.printf("get address invalid param, len(%d)!=5\n", len);
	       				continue;
	       			}
	       			addrlen=sizeof(addr);
	       			if(getpeername(sockfd,(struct sockaddr *)&addr,&addrlen)==-1 || inet_ntop(AF_INET,&(addr.sin_addr),address,INET_ADDRSTRLEN)==NULL){
	       				closeClient(id,sockfd);
	       				logger.printf("get address failed\n");
	       				continue;
	       			}
	       			len_2=strlen(address);
	       			len=htonl(4+len_2);
	       			if(send(sockfd,&len,4,MSG_NOSIGNAL)==-1 || send(sockfd,address,len_2,MSG_NOSIGNAL)==-1){
	       				closeClient(id,sockfd);
	       				logger.printf("push address failed,broken link\n");
	       			}
	       		} else if(cmd==6){ //broadcast
	       			if(len<5){
	       				logger.printf("broadcast :len(%d)<9\n",len);
	       				closeClient(id,sockfd);
	       				continue;
	       			}
	       			const char *content=buff+5;
	       			len-=5;
	       			len_2=htonl(len+4);
	       			for(const auto& client : bound_clients ) {
						if(sockfd == client.second) continue;
	       				if(send(client.second,&len_2,4,MSG_NOSIGNAL)==-1 || send(client.second,content,len,MSG_NOSIGNAL)==-1){
	       					bound_clients.erase(client.first);
	       					close(client.second);
	       					logger.printf("(id:%ld) push failed,broken link\n",client.first);
	       					logger.printf("errno:%d\n", errno);
	       				}
				    }
				    for(const auto& client : unbound_clients ) {
						if(sockfd == client) continue;
	       				if(send(client,&len_2,4,MSG_NOSIGNAL)==-1 || send(client,content,len,MSG_NOSIGNAL)==-1){
	       					unbound_clients.erase(client);
	       					close(client);
	       					logger.printf("(fd:%ld) push failed,broken link\n",client);
	       					logger.printf("errno:%d\n", errno);
	       				}
				    }
	       		} else {//unknown cmd
	       			logger.printf("unknow cmd: %d\n", cmd);
	       			closeClient(id,sockfd);
	       		}
	       }
	   }
	}
}