#include "json.h"
#include "push.h"
#include <sys/ioctl.h>
#include <string.h>


void *accept_tcp(void *arg){
	int epollfd_socket=((common_data *)arg)->epollfd_socket;
	Log *logger=((common_data *)arg)->logger;
	int servfd,sockfd;
	struct epoll_event ev;
	ev.events = EPOLLIN;
	struct KeepConfig cfg = { 20, 2, 5};
	if((servfd=initTcpServer(SERVER_PORT))<0){
		logger->printf("inital tcp server failed\n");
	}else{
		logger->printf("initial tcp server success\n");
		while((sockfd=accept(servfd,NULL,NULL))>=0){
			set_tcp_keepalive_cfg(sockfd, &cfg);
			ev.data.fd = sockfd;
			if(epoll_ctl(epollfd_socket, EPOLL_CTL_ADD, sockfd, &ev)==-1){
				logger->printf("add socket to epollfd failed. errno:%d\n",errno);
			}
		}
		close(servfd);
		logger->printf("tcp server accept failed. errno:%d\n",errno);
	}
}

void *read_socket(void * arg){
	Log *logger=((common_data *)arg)->logger;
	rb_tree *data=((common_data *)arg)->data;

	unsigned int epollfd_socket=((common_data *)arg)->epollfd_socket;
	unsigned int epollfd_socket_et=((common_data *)arg)->epollfd_socket_et;
	unsigned int epollfd_client=((common_data *)arg)->epollfd_client;
	linked_list *list_socket=((common_data *)arg)->list_socket;
	char buff[4+MAX_MESSAGE_SIZE];
	char ack_ok[7]={0,0,0,3,'2','0','0'};
	client *p=0;
	unsigned long id;
	unsigned int len,len_d,num,sockfd;
	struct epoll_event events[1];
	struct epoll_event ev;

	for(;;){
		if(epoll_wait(epollfd_socket, events, 1, -1)<1){
			logger->printf("epoll wait error. errno:%d\n",errno);
		}else{
			sockfd=events[0].data.fd;
			//check whether data is ready for process
			if(recv(sockfd,&len_d,4,MSG_DONTWAIT|MSG_PEEK)!=4){
				ev.data.fd=sockfd;
				ev.events=EPOLLIN|EPOLLIN;
				if(epoll_ctl(epollfd_socket, EPOLL_CTL_DEL, sockfd, NULL)==-1 || epoll_ctl(epollfd_socket_et, EPOLL_CTL_ADD, sockfd, &ev)==-1){
					close(sockfd);
					logger->printf("move socket(%d) from epollfd_socket to epollfd_socket_et failed. errno:%d\n",sockfd,errno);
				}else{
					list_socket->append(sockfd);
				}
			} else if(len_d>MAX_MESSAGE_SIZE){
				close(sockfd);
				logger->printf("read socket(%d) len(%d) > %d\n", sockfd,len_d,MAX_MESSAGE_SIZE);
			}else if(ioctl(sockfd,FIONREAD,&len)==-1){
				close(sockfd);
				logger->printf("ioctl failed\n");
			}else if(len<len_d+4){
				ev.data.fd=sockfd;
				ev.events=EPOLLIN|EPOLLIN;
				if(epoll_ctl(epollfd_socket, EPOLL_CTL_DEL, sockfd, NULL)==-1 || epoll_ctl(epollfd_socket_et, EPOLL_CTL_ADD, sockfd, &ev)==-1){
					close(sockfd);
					logger->printf("move socket(%d) from epollfd_socket to epollfd_socket_et failed. errno:%d\n",sockfd,errno);
				}else{
					list_socket->append(sockfd);
				}
			}else if(recv(sockfd,buff,len,MSG_DONTWAIT)!=len){
				close(sockfd);
				logger->printf("read socket len!=%d\n",len);
			}else{
				//start process
				unsigned char cmd=buff[4];
				if(cmd==1){//bind
					if(len!=13){
						close(sockfd);
						logger->printf("cmd 1 :len!=13\n");
					}else{
						memcpy(&id,buff+5,8);
						data->insert(id,(p=new client(id,sockfd)));
						if(send(sockfd,ack_ok,7,MSG_NOSIGNAL)<0){
							p->mutex_unlock();
							data->remove(id);
							logger->printf("(id:%ld) send bind response failed\n", id);
						}else{
							p->mutex_unlock();
							logger->printf("(id:%ld) bind success\n", id);
							ev.data.u64=id;
							ev.events = EPOLLIN;
							if(epoll_ctl(epollfd_socket, EPOLL_CTL_DEL, sockfd, NULL)==-1 || epoll_ctl(epollfd_client, EPOLL_CTL_ADD, sockfd, &ev)==-1){
								data->remove(id);
								logger->printf("move client(%ld) from epollfd_socket to epollfd_client failed. errno:%d\n",id,errno);
							}
						}
					}
				}else if(cmd==2){//push
					memcpy(&num,buff+5,4);
					if(len<(9+num*8)){
						close(sockfd);
						logger->printf("cmd 2 :len(%d) <%d\n", len,9+num*8);
					}else{
						const char *content=buff+9+num*8;
						len-=9+num*8;
						for(int i=0;i<num;++i){
							memcpy(&id,buff+9+8*i,8);
							if(p=(client *)data->search(id)){
								if(send(p->fd,&len,4,MSG_NOSIGNAL)<0 || send(p->fd,content,len,MSG_NOSIGNAL)<0){
									p->mutex_unlock();
									data->remove(id);
									logger->printf("(id:%ld) push failed,broken link\n",id);
								}else{
									p->mutex_unlock();
									logger->printf("(id:%ld) push success\n",id);
								}
							}
						}
					}
				}else{
					close(sockfd);
					logger->printf("socket(%ld) unknown cmd:%d\n", sockfd,cmd);
				}
			}
		}
	}
}

void *read_socket_et(void *arg){
	Log *logger=((common_data *)arg)->logger;

	unsigned int epollfd_socket=((common_data *)arg)->epollfd_socket;
	unsigned int epollfd_socket_et=((common_data *)arg)->epollfd_socket_et;
	linked_list *list_socket=((common_data *)arg)->list_socket;
	unsigned int len,len_d,sockfd;
	list_item *item=0;
	struct epoll_event events[1];
	struct epoll_event ev;
	ev.events=EPOLLIN;
	

	for(;;){
		long wait_time=READ_TIME_OUT;
		item=list_socket->pop();
		if(item){
			wait_time=item->timestamp+READ_TIME_OUT-getCurrentTime();
			if(wait_time<=0){
				sockfd=item->data;
				delete item;
				if(recv(sockfd,&len_d,4,MSG_DONTWAIT|MSG_PEEK)!=4 || len_d>MAX_MESSAGE_SIZE || 
						ioctl(sockfd,FIONREAD,&len)==-1 || len<len_d+4 || epoll_ctl(epollfd_socket_et, EPOLL_CTL_DEL, sockfd, NULL)==-1
						|| epoll_ctl(epollfd_socket, EPOLL_CTL_ADD, sockfd, &ev)==-1){
					close(sockfd);
				}
				continue;
			}
			list_socket->append_to_head(item);
		}
		if(epoll_wait(epollfd_socket_et,events,1,wait_time)<1){
			continue;
		}else{
			sockfd=events[0].data.fd;
			delete list_socket->get(sockfd);
			if(recv(sockfd,&len_d,4,MSG_DONTWAIT|MSG_PEEK)!=4){
				list_socket->append(sockfd);
			} else if(len_d>MAX_MESSAGE_SIZE){
				close(sockfd);
				logger->printf("read socket(%d) len(%d) > %d\n", sockfd,len_d,MAX_MESSAGE_SIZE);
			}else if(ioctl(sockfd,FIONREAD,&len)==-1){
				close(sockfd);
				logger->printf("ioctl failed\n");
			}else if(len<len_d+4){
				list_socket->append(sockfd);
			}else{//socket is ready for process
				ev.data.fd=sockfd;
				if(epoll_ctl(epollfd_socket_et, EPOLL_CTL_DEL, sockfd, NULL)==-1 || epoll_ctl(epollfd_socket, EPOLL_CTL_ADD, sockfd, &ev)==-1){
					close(sockfd);
					logger->printf("move socket(%d) from epollfd_socket_et to epollfd_socket failed. errno:%d\n",sockfd,errno);
				}
			}
		}
	}
}


void *read_client(void * arg){
	Log *logger=((common_data *)arg)->logger;
	rb_tree *data=((common_data *)arg)->data;

	int epollfd_client=((common_data *)arg)->epollfd_client;
	int len;
	char buff[MAX_MESSAGE_SIZE];
	long id;
	client *p=0;
	struct epoll_event ev[1];
	for(;;){
		if(epoll_wait(epollfd_client, ev, 1, -1)<1){
			logger->printf("epollfd_client wait error. errno:%d\n",errno);
		}else{
			id=ev[0].data.u64;
			if(!(p=(client *)data->search(id))){
				continue;
			}
			len=0;
			int recv_len=0;
			if((recv_len=read(p->fd,&len,4))!=4){
				logger->printf("client(%ld) read len failed:%d\n", id,recv_len);
				p->mutex_unlock();
				data->remove(id);
			} else if(len>=MAX_MESSAGE_SIZE){
				logger->printf("client(%ld) len(%d) >= %d\n", id,len,MAX_MESSAGE_SIZE);
				p->mutex_unlock();
				data->remove(id);
			}else if((recv_len=read(p->fd,buff,len))!=len){
				buff[recv_len]=0;
				logger->printf("client(%ld) read failed:%s\n", id,buff);
				p->mutex_unlock();
				data->remove(id);
			}else{
				p->mutex_unlock();
				buff[len]=0;
				logger->printf("read client(%ld):%s\n",id,buff);
				try{
					jsonObject json(buff,len);
					int cmd=json.getInt("cmd");
					if(cmd==2){//push
						jsonArray *id_arr=json.getJsonArray("ids");
						const char *content=json.getString("content");
						len=strlen(content);
						for(int i=0;i<id_arr->length();++i){
							long t_id=id_arr->getLong(i);
							if(p=(client *)data->search(t_id)){
								if(send(p->fd,&len,4,MSG_NOSIGNAL)<0 || send(p->fd,content,len,MSG_NOSIGNAL)<0){
									p->mutex_unlock();
									data->remove(t_id);
									logger->printf("(id:%ld) push failed,broken link\n",t_id);
								}else{
									p->mutex_unlock();
									logger->printf("(id:%ld) push success\n",t_id);
								}
							}
						}
					}else{
						data->remove(id);
						logger->printf("client(%ld) unknown cmd:\n", id,cmd);
					}
				}catch(std::runtime_error err){
					data->remove(id);
					logger->printf("client(%ld), json error:%s\n",id,err.what());
				}
			}
		}
	}
}
void *read_client_et(void * arg){

}



int main(int argc,char *argv[]){
	daemonize("push_server");

	char *path=getcwd(NULL,0);
	if(path==NULL){
		return -1;
	}
	Log logger(path);
	delete path;

	pthread_t tid_accept_tcp,tid_read_socket,tid_read_socket_et,tid_read_client,tid_read_client_et;
	rb_tree data;
	int epollfd_socket=epoll_create1(0);
	int epollfd_socket_et=epoll_create1(0);
	int epollfd_client=epoll_create1(0);
	int epollfd_client_et=epoll_create1(0);
	linked_list list_socket;
	linked_list list_client;

	common_data c_data={&data,&logger,epollfd_socket,epollfd_socket_et,epollfd_client,epollfd_client_et,&list_socket,&list_client};

	if(pthread_create(&tid_accept_tcp,NULL,accept_tcp,&c_data)==0){
		logger.printf("create accept tcp thread success\n");
	}else{
		logger.printf("create accept tcp thread failed\n");
		syslog(LOG_ERR,"create accept tcp thread failed\n");
	}

	if(pthread_create(&tid_read_socket,NULL,read_socket,&c_data)==0){
		logger.printf("create read socket thread success\n");
	}else{
		logger.printf("create read socket thread failed\n");
		syslog(LOG_ERR,"create read socket thread failed\n");
	}

	if(pthread_create(&tid_read_socket_et,NULL,read_socket_et,&c_data)==0){
		logger.printf("create read socket et thread success\n");
	}else{
		logger.printf("create read socket et thread failed\n");
		syslog(LOG_ERR,"create read socket et thread failed\n");
	}

	if(pthread_create(&tid_read_client,NULL,read_client,&c_data)==0){
		logger.printf("create read client thread success\n");
	}else{
		logger.printf("create read client thread failed\n");
		syslog(LOG_ERR,"create read client client thread failed\n");
	}

	if(pthread_create(&tid_read_client_et,NULL,read_client_et,&c_data)==0){
		logger.printf("create read client et thread success\n");
	}else{
		logger.printf("create read client et thread failed\n");
		syslog(LOG_ERR,"create read client et client thread failed\n");
	}

	logger.flush();

	pthread_join(tid_accept_tcp,NULL);
	pthread_join(tid_read_socket,NULL);
	pthread_join(tid_read_socket_et,NULL);
	pthread_join(tid_read_client,NULL);
	pthread_join(tid_read_client_et,NULL);
}