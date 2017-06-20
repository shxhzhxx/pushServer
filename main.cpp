#include "json.h"
#include "push.h"


void *accept_thread(void *arg){
	int epollfd_bind=((common_data *)arg)->epollfd_bind;
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
			if(epoll_ctl(epollfd_bind, EPOLL_CTL_ADD, sockfd, &ev)==-1){
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
	int sockfd,len;
	char buff[MAX_MESSAGE_SIZE];
	long id;
	client *p=0;
	struct epoll_event ev[1];

	for(;;){
		if(epoll_wait(epollfd_bind, ev, 1, -1)<1){
			log->printf("epoll wait error. errno:%d\n",errno);
			syslog(LOG_ERR,"epoll wait error. errno:%d\n",errno);
		}else{
			sockfd=ev[0].data.fd;
			if((len=read(sockfd,buff,MAX_MESSAGE_SIZE))<=0){
				log->printf("socket read -1\n", sockfd);
				close(sockfd);
			}else{
				try{
					jsonObject json(buff,len);
					switch(json.getInt("cmd")){
						case 1: //bind
							id=json.getLong("id");
							data->insert(id,(p=new client(id,sockfd)));
							if(send(sockfd,"200",3,MSG_NOSIGNAL)<0){
								p->mutex_unlock();
								data->remove(id);
								log->printf("(id:%ld) send bind response failed\n", id);
							}else{
								p->mutex_unlock();
								log->printf("(id:%ld) bind success\n", id);
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
							close(sockfd);
					}
				}catch(std::runtime_error err){
					close(sockfd);
					log->printf("read thread, json error:%s\n",err.what());
				}
			}
		}
	}
}


void *read_client_thread(void * arg){

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