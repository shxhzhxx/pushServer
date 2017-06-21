#include "json.h"
#include "push.h"


void *accept_thread(void *arg){
	int epollfd=((common_data *)arg)->epollfd;
	Log *logger=((common_data *)arg)->logger;
	int servfd,sockfd;
	struct epoll_event ev;
	ev.events = EPOLLIN;
	struct KeepConfig cfg = { 20, 2, 5};
	if((servfd=initTcpServer(SERVER_PORT))<0){
		logger->printf("inital accept socket failed\n");
		syslog(LOG_ERR,"inital accept socket failed\n");
	}else{
		logger->printf("initial accept socket success\n");
		while((sockfd=accept(servfd,NULL,NULL))>=0){
			set_tcp_keepalive_cfg(sockfd, &cfg);
			ev.data.fd = sockfd;
			if(epoll_ctl(epollfd, EPOLL_CTL_ADD, sockfd, &ev)==-1){
				logger->printf("add socket to epollfd failed. errno:%d\n",errno);
				syslog(LOG_ERR,"add socket to epollfd failed. errno:%d\n",errno);
			}
		}
		close(servfd);
		logger->printf("accept socket broken. errno:%d\n",errno);
		syslog(LOG_ERR,"accept socket broken. errno:%d\n",errno);
	}
}

void *read_thread(void * arg){
	Log *logger=((common_data *)arg)->logger;
	rb_tree *data=((common_data *)arg)->data;

	int epollfd=((common_data *)arg)->epollfd;
	int epollfd_client=((common_data *)arg)->epollfd_client;
	int sockfd,len;
	char buff[MAX_MESSAGE_SIZE];
	long id;
	client *p=0;
	struct epoll_event events[1];
	struct epoll_event ev;
	ev.events = EPOLLIN;

	for(;;){
		if(epoll_wait(epollfd, events, 1, -1)<1){
			logger->printf("epoll wait error. errno:%d\n",errno);
			syslog(LOG_ERR,"epoll wait error. errno:%d\n",errno);
		}else{
			sockfd=events[0].data.fd;
			len=0;
			int recv_len=0;
			if(read(sockfd,&len,4)!=4){
				logger->printf("socket(%d) read len failed\n", sockfd);
				close(sockfd);
			} else if(len>=MAX_MESSAGE_SIZE){
				logger->printf("socket(%d) len(%d) >= %d\n", sockfd,len,MAX_MESSAGE_SIZE);
				close(sockfd);
			}else if((recv_len=read(sockfd,buff,len))!=len){
				buff[recv_len]=0;
				logger->printf("socket(%d) read failed:%s\n", sockfd,buff);
				close(sockfd);
			}else{
				try{
					jsonObject json(buff,len);
					int cmd=json.getInt("cmd");
					if(cmd==1){//bind
						id=json.getLong("id");
						data->insert(id,(p=new client(id,sockfd)));
						if(send(sockfd,"200",3,MSG_NOSIGNAL)<0){
							p->mutex_unlock();
							data->remove(id);
							logger->printf("(id:%ld) send bind response failed\n", id);
							syslog(LOG_ERR,"(id:%ld) send bind response failed\n", id);
						}else{
							p->mutex_unlock();
							logger->printf("(id:%ld) bind success\n", id);
							ev.data.u64=id;
							if(epoll_ctl(epollfd, EPOLL_CTL_DEL, sockfd, NULL)==-1){
								data->remove(id);
								logger->printf("remove client(%ld) from epollfd failed. errno:%d\n",id,errno);
								syslog(LOG_ERR,"remove client(%ld) from epollfd failed. errno:%d\n",id,errno);
							}else if(epoll_ctl(epollfd_client, EPOLL_CTL_ADD, sockfd, &ev)==-1){
								data->remove(id);
								logger->printf("add client(%ld) to epollfd_client failed. errno:%d\n",id,errno);
								syslog(LOG_ERR,"add client(%ld) to epollfd_client failed. errno:%d\n",id,errno);
							}
						}
						p=0;
					}else if(cmd==2){//push
						buff[len]=0;
						// logger->printf("push request:%s\n",buff);
						jsonArray *id_arr=json.getJsonArray("ids");
						const char *content=json.getString("content");
						len=strlen(content);
						for(int i=0;i<id_arr->length();++i){
							id=id_arr->getLong(i);
							if(p=(client *)data->search(id)){
								if(send(p->fd,&len,4,MSG_NOSIGNAL)<0 || send(p->fd,content,len,MSG_NOSIGNAL)<0){
									p->mutex_unlock();
									data->remove(id);
									logger->printf("(id:%ld) push failed,broken link\n",id);
								}else{
									p->mutex_unlock();
									// logger->printf("(id:%ld) push success\n",id);
								}
							}
						}
					}else{
						logger->printf("socket(%ld) unknown cmd:\n", sockfd,cmd);
						syslog(LOG_ERR,"socket(%ld) unknown cmd:\n", sockfd,cmd);
						close(sockfd);
					}
				}catch(std::runtime_error err){
					close(sockfd);
					logger->printf("socket(%ld), json error:%s\n",sockfd,err.what());
				}
			}
		}
	}
}


void *read_client_thread(void * arg){
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
			syslog(LOG_ERR,"epollfd_client wait error. errno:%d\n",errno);
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
				// logger->printf("read client(%ld):%s\n",id,buff);
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
									// logger->printf("(id:%ld) push success\n",t_id);
								}
							}
						}
					}else{
						data->remove(id);
						logger->printf("client(%ld) unknown cmd:\n", id,cmd);
						syslog(LOG_ERR,"client(%ld) unknown cmd:\n", id,cmd);
					}
				}catch(std::runtime_error err){
					data->remove(id);
					logger->printf("client(%ld), json error:%s\n",id,err.what());
					syslog(LOG_ERR,"client(%ld), json error:%s\n",id,err.what());
				}
			}
		}
	}
}



int main(int argc,char *argv[]){
	daemonize("push_server");

	char *path=getcwd(NULL,0);
	if(path==NULL){
		return -1;
	}
	Log logger(path);
	delete path;

	pthread_t tid_accept,tid_read,tid_read_client;
	rb_tree data;
	int epollfd=epoll_create1(0);
	int epollfd_et=epoll_create1(0);
	int epollfd_client=epoll_create1(0);
	int epollfd_client_et=epoll_create1(0);

	common_data c_data={&data,&logger,epollfd,epollfd_client};

	if(pthread_create(&tid_accept,NULL,accept_thread,&c_data)==0){
		logger.printf("create accept thread success\n");
	}else{
		logger.printf("create accept thread failed\n");
		syslog(LOG_ERR,"create accept thread failed\n");
	}

	if(pthread_create(&tid_read,NULL,read_thread,&c_data)==0){
		logger.printf("create read thread success\n");
	}else{
		logger.printf("create read thread failed\n");
		syslog(LOG_ERR,"create read thread failed\n");
	}

	if(pthread_create(&tid_read_client,NULL,read_client_thread,&c_data)==0){
		logger.printf("create read client thread success\n");
	}else{
		logger.printf("create read thread failed\n");
		syslog(LOG_ERR,"create read client thread failed\n");
	}

	logger.flush();

	pthread_join(tid_accept,NULL);
	pthread_join(tid_read,NULL);
	pthread_join(tid_read_client,NULL);
}