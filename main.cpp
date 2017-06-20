#include "json.h"
#include "push.h"


void *accept_thread(void *arg){
	int epollfd=((common_data *)arg)->epollfd;
	log *log=((common_data *)arg)->log;
	int servfd,sockfd;
	struct epoll_event ev;
	ev.events = EPOLLIN;
	struct KeepConfig cfg = { 20, 2, 5};
	if((servfd=initTcpServer(SERVER_PORT))<0){
		log->printf("inital accept socket failed\n");
		syslog(LOG_ERR,"inital accept socket failed\n");
	}else{
		log->printf("initial accept socket success\n");
		while((sockfd=accept(servfd,NULL,NULL))>=0){
			set_tcp_keepalive_cfg(sockfd, &cfg);
			ev.data.fd = sockfd;
			if(epoll_ctl(epollfd, EPOLL_CTL_ADD, sockfd, &ev)==-1){
				log->printf("add socket to epollfd failed. errno:%d\n",errno);
				syslog(LOG_ERR,"add socket to epollfd failed. errno:%d\n",errno);
			}
		}
		close(servfd);
		log->printf("accept socket broken. errno:%d\n",errno);
		syslog(LOG_ERR,"accept socket broken. errno:%d\n",errno);
	}
}

void *read_thread(void * arg){
	log *log=((common_data *)arg)->log;
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
			log->printf("epoll wait error. errno:%d\n",errno);
			syslog(LOG_ERR,"epoll wait error. errno:%d\n",errno);
		}else{
			sockfd=events[0].data.fd;
			if((len=read(sockfd,buff,MAX_MESSAGE_SIZE))<=0){
				log->printf("socket(%d) read failed\n", sockfd);
				close(sockfd);
			}else{
				try{
					jsonObject json(buff,len);
					int cmd=json.getInt("cmd");
					switch(cmd){
						case 1: //bind
							id=json.getLong("id");
							data->insert(id,(p=new client(id,sockfd)));
							if(send(sockfd,"200",3,MSG_NOSIGNAL)<0){
								p->mutex_unlock();
								data->remove(id);
								log->printf("(id:%ld) send bind response failed\n", id);
								syslog(LOG_ERR,"(id:%ld) send bind response failed\n", id);
							}else{
								p->mutex_unlock();
								log->printf("(id:%ld) bind success\n", id);
								ev.data.uint64_t=id;
								if(epoll_ctl(epollfd, EPOLL_CTL_DEL, sockfd, NULL)==-1){
									data->remove(id);
									log->printf("remove client(%ld) from epollfd failed. errno:%d\n",id,errno);
									syslog(LOG_ERR,"remove client(%ld) from epollfd failed. errno:%d\n",id,errno);
								}else if(epoll_ctl(epollfd_client, EPOLL_CTL_ADD, sockfd, &ev)==-1){
									data->remove(id);
									log->printf("add client(%ld) to epollfd_client failed. errno:%d\n",id,errno);
									syslog(LOG_ERR,"add client(%ld) to epollfd_client failed. errno:%d\n",id,errno);
								}
							}
							p=0;
						break;
						case 2: //push
							buff[len]=0;
							log->printf("push request:%s\n",buff);
							jsonArray *id_arr=json.getJsonArray("ids");
							const char *content=json.getString("content");
							for(int i=0;i<id_arr->length();++i){
								id=id_arr->getLong(i);
								if(p=data->search(id)){
									if(send(p->fd,content,strlen(content),MSG_NOSIGNAL)<0){
										p->mutex_unlock();
										data->remove(id);
										log->printf("(id:%ld) push failed,broken link\n",id);
									}else{
										p->mutex_unlock();
										log->printf("(id:%ld) push success\n",id);
									}
								}
							}
						break;
						default:
							log->printf("socket(%ld) unknown cmd:\n", sockfd,cmd);
							syslog(LOG_ERR,"socket(%ld) unknown cmd:\n", sockfd,cmd);
							close(sockfd);
					}
				}catch(std::runtime_error err){
					close(sockfd);
					log->printf("socket(%ld), json error:%s\n",sockfd,err.what());
				}
			}
		}
	}
}


void *read_client_thread(void * arg){
	log *log=((common_data *)arg)->log;
	rb_tree *data=((common_data *)arg)->data;

	int epollfd_client=((common_data *)arg)->epollfd_client;
	int len;
	char buff[MAX_MESSAGE_SIZE];
	long id;
	client *p=0;
	struct epoll_event ev[1];
	for(;;){
		if(epoll_wait(epollfd_client, ev, 1, -1)<1){
			log->printf("epollfd_client wait error. errno:%d\n",errno);
			syslog(LOG_ERR,"epollfd_client wait error. errno:%d\n",errno);
		}else{
			id=ev[0].data.uint64_t;
			if(!(p=data->search(id))){
				continue;
			}
			if((len=read(p->fd,buff,MAX_MESSAGE_SIZE))<=0){
				p->mutex_unlock();
				data->remove(id);
				log->printf("read client(%ld) error\n", id);
				syslog(LOG_ERR,"read client(%ld) error\n", id);
			}else{
				p->mutex_unlock();
				try{
					jsonObject json(buff,len);
					int cmd=json.getInt("cmd");
					switch(cmd){
						case 2: //push
							buff[len]=0;
							log->printf("push request:%s\n",buff);
							jsonArray *id_arr=json.getJsonArray("ids");
							const char *content=json.getString("content");
							for(int i=0;i<id_arr->length();++i){
								long t_id=id_arr->getLong(i);
								if(p=data->search(t_id)){
									if(send(p->fd,content,strlen(content),MSG_NOSIGNAL)<0){
										p->mutex_unlock();
										data->remove(t_id);
										log->printf("(id:%ld) push failed,broken link\n",t_id);
									}else{
										p->mutex_unlock();
										log->printf("(id:%ld) push success\n",t_id);
									}
								}
							}
						break;
						default:
							data->remove(id);
							log->printf("client(%ld) unknown cmd:\n", id,cmd);
							syslog(LOG_ERR,"client(%ld) unknown cmd:\n", id,cmd);
					}
				}catch(std::runtime_error err){
					data->remove(id);
					log->printf("client(%ld), json error:%s\n",id,err.what());
					syslog(LOG_ERR,"client(%ld), json error:%s\n",id,err.what());
				}
			}
		}
	}
}



int main(int argc,char *argv[]){
	daemonize("push_server");

	pthread_t tid_accept,tid_read,tid_read_client;
	rb_tree data;
	int epollfd=epoll_create1(0);
	int epollfd_client=epoll_create1(0);


	char *path=getcwd(NULL,0);
	if(path==NULL){
		return -1;
	}
	log log(path);
	delete path;


	common_data c_data={&data,&log,epollfd,epollfd_client};

	if(pthread_create(&tid_accept,NULL,accept_thread,&c_data)==0){
		log.printf("create accept thread success\n");
	}else{
		log.printf("create accept thread failed\n");
		syslog(LOG_ERR,"create accept thread failed\n");
	}

	if(pthread_create(&tid_read,NULL,read_thread,&c_data)==0){
		log.printf("create read thread success\n");
	}else{
		log.printf("create read thread failed\n");
		syslog(LOG_ERR,"create read thread failed\n");
	}

	if(pthread_create(&tid_read_client,NULL,read_client_thread,&c_data)==0){
		log.printf("create read client thread success\n");
	}else{
		log.printf("create read thread failed\n");
		syslog(LOG_ERR,"create read client thread failed\n");
	}

	log.flush();

	pthread_join(tid_accept,NULL);
	pthread_join(tid_read,NULL);
	pthread_join(tid_read_client,NULL);
}