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
				close(sockfd);
			}else{
				try{
					jsonObject json(buff,len);
					switch(json.getInt("cmd")){
						case 1: //bind
							id=json.getLong("id");
							if(data->insert_try(id,(p=new client(id,sockfd)))==-1){
								send(sockfd,"301",3,MSG_NOSIGNAL); //绑定失败
								close(sockfd);
								log->printf("(id:%ld) bind failed, id conflict\n",id);
							}else{
								if(send(sockfd,"200",3,MSG_NOSIGNAL)<0){
									p->mutex_unlock();
									data->remove(id);
									log->printf("(id:%ld) send response failed\n", id);
								}else{
									p->mutex_unlock();
									log->printf("(id:%ld) bind success\n", id);
								}
								p=0;
							}
						break;
						case 2: //push
							buff[len]=0;
							log->printf("push request:%s\n",buff);
							jsonArray *id_arr=json.getJsonArray("ids");
							const char *content=json.getString("content");
							for(int i=0;i<id_arr->length();++i){//check param,ids array's element shold be number'
								id=id_arr->getLong(i);
							}
						break;
						default:
							close(sockfd);
					}
				}catch(std::runtime_error err){
					close(sockfd);
					log->printf("json error:%s\n",err.what());
				}
			}
		}
	}
}


void *push_request(void * arg){
	log *logger_p=((common_data *)arg)->logger_p;
	rb_tree *data_tree=((common_data *)arg)->data_tree;
	int push_epollfd=((common_data *)arg)->push_epollfd;
	link_list *link=((common_data *)arg)->link;
	link_list *callback_link=((common_data *)arg)->callback_link;
	struct epoll_event events[1];

	int sockfd;
	user_data *p=0;
	char buff[MAX_MESSAGE_SIZE];
	int len,search_ret;
	long id;

	for (;;) {
		if(epoll_wait(push_epollfd, events, 1, -1)==1){
			sockfd=events[0].data.fd;
			if((len=read(sockfd,buff,MAX_MESSAGE_SIZE))>0){
				buff[len]=0;
				logger_p->printf("push request:%s\n",buff);
				try{
					jsonObject json(buff,len);
					jsonArray *id_arr=json.getJsonArray("ids");
					for(int i=0;i<id_arr->length();++i){//check param,ids array's element shold be number'
						id_arr->getLong(i);
					}
					const char *content=json.getJsonObject("content")->toString();
					int callback_type=json.has("callback_type")?json.getInt("callback_type"):0;
					int push_id=0;
					const char *callback_url=0;
					if(callback_type==1){
						push_id=json.getInt("push_id");
						callback_url=json.getString("callback_url");
					}
					for(int i=0;i<id_arr->length();++i){//check param
						id=id_arr->getLong(i);
						switch(callback_type){
							case 0: //无回调
							case 1: //url回调
								search_ret=data_tree->search_try(id,(satellite **)&p);
								if(search_ret==0){
									if(send(p->fd,content,strlen(content),MSG_NOSIGNAL)<0){ //push failed,broken link
										p->mutex_unlock();
										data_tree->remove(id);
										logger_p->printf("(id:%ld) push failed,broken link\n",id);
									}else{
										logger_p->printf("(id:%ld) push success,waiting ack...\n",id);
										link->append_message(new message(content,callback_type,p,push_id,callback_url));
									}
								}else if(search_ret==1){ //之前的推送尚未完成     或      这个id刚被插入，还没解锁（这时这条信息会被延迟）
									p->append_message(new message(content,callback_type,p,push_id,callback_url));
									logger_p->printf("(id:%ld) link block,waiting ack,append message\n",id);
								}else{
									if(callback_type==1){
										callback_link->append_message(new message(content,callback_type,id,push_id,callback_url));
									}
									logger_p->printf("(id:%ld) push failed,no match id\n",id);
									//没有匹配的id
								}
								p=0;
								close(sockfd);
							break;
							case 2: //长连接回调
								close(sockfd);//暂不支持
							break;
							default: //参数错误
								close(sockfd);
						}
					}
				}catch(std::runtime_error err){
					close(sockfd);
					logger_p->printf("json error:%s\n",err.what());
				}
			}else{
				close(sockfd);
			}
		}else{
			logger_p->printf("epoll_wait error\n");
			syslog(LOG_ERR,"epoll_wait error\n");
		}
	}
}

int main(int argc,char *argv[]){
	daemonize("push_server");

	pthread_t tid_accept,tid_read;
	rb_tree data;
	int epollfd=epoll_create1(0);


	char *path=getcwd(NULL,0);
	if(path==NULL){
		return -1;
	}
	log log(path);
	delete path;


	common_data c_data={&data,&log,epollfd};

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

	log.flush();

	pthread_join(tid_accept,NULL);
	pthread_join(tid_read,NULL);
}