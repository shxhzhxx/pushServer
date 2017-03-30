#include "json.h"
#include "push.h"


void *bind_tcp_link(void * arg){
	int bind_epollfd=((common_data *)arg)->bind_epollfd;
	logger *logger_p=((common_data *)arg)->logger_p;
	int servfd,sockfd;
	struct epoll_event ev;
	ev.events = EPOLLIN;
	struct KeepConfig cfg = { 20, 2, 5};
	if((servfd=initTcpServer(BIND_SERVER_PORT))>0){
		logger_p->printf("initial bind server success\n");
		while((sockfd=accept(servfd,NULL,NULL))>=0){
			set_tcp_keepalive_cfg(sockfd, &cfg);
			ev.data.fd = sockfd;
			if(epoll_ctl(bind_epollfd, EPOLL_CTL_ADD, sockfd, &ev)==-1){
				logger_p->printf("epoll_ctl failed 1 errno:%d\n",errno);
				syslog(LOG_ERR,"epoll_ctl failed 1 errno:%d\n",errno);
			}
		}
		close(servfd);
	}else{
		logger_p->printf("inital bind server error\n");
		syslog(LOG_ERR,"inital bind server error\n");
	}
}

void *bind_request(void * arg){
	logger *logger_p=((common_data *)arg)->logger_p;
	rb_tree *data_tree=((common_data *)arg)->data_tree;
	int bind_epollfd=((common_data *)arg)->bind_epollfd;
	int epollfd=((common_data *)arg)->epollfd;
	int sockfd,len;
	char buff[1024];
	long id;
	user_data *p=0;
	struct epoll_event ev;
	ev.events = EPOLLIN;

	struct epoll_event events[1];
	for (;;) {
		if(epoll_wait(bind_epollfd, events, 1, -1)==1){
			sockfd=events[0].data.fd;
			if(epoll_ctl(bind_epollfd, EPOLL_CTL_DEL, sockfd, NULL)==-1){
				logger_p->printf("epoll_ctl failed 2 errno:%d\n",errno);
				syslog(LOG_ERR,"epoll_ctl failed 2 errno:%d\n",errno);
			}
			if((len=read(sockfd,buff,1024))>0){
				try{
					jsonObject json(buff,len);
					id=json.getLong("id");
					switch(json.getInt("device_type")){
						case 1: //android
							if(data_tree->insert_try(id,(p=new user_data(id,sockfd)),true)==-1){
								send(sockfd,"301",3,MSG_NOSIGNAL); //绑定失败
								close(sockfd);
								logger_p->printf("(id:%ld) bind error id conflict\n",id);
							}else{
								if(send(sockfd,"200",3,MSG_NOSIGNAL)<0){
									p->mutex_unlock();
									data_tree->remove(id);
									logger_p->printf("(id:%ld) bind error no response\n", id);
								}else{
									ev.data.ptr=p;
									if (epoll_ctl(epollfd, EPOLL_CTL_ADD, p->fd, &ev) == -1) {
										logger_p->printf("(id:%ld) epoll_ctl failed 4 errno:%d\n",id,errno);
										syslog(LOG_ERR,"(id:%ld) epoll_ctl failed 4 errno:%d\n",id,errno);
									}
									p->mutex_unlock();
									logger_p->printf("(id:%ld) bind success\n", id);
								}
								p=0;
							}
						break;
						case 2: //ios

						break;
						default:
							close(sockfd);
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

void *push_tcp_link(void * arg){
	logger *logger_p=((common_data *)arg)->logger_p;
	int push_epollfd=((common_data *)arg)->push_epollfd;
	int servfd,sockfd;
	struct epoll_event ev;
	ev.events = EPOLLIN;
	struct KeepConfig cfg = { 20, 2, 5};
	if((servfd=initTcpServer(PUSH_SERVER_PORT))>0){
		logger_p->printf("initial push server success\n");
		while((sockfd=accept(servfd,NULL,NULL))>=0){
			set_tcp_keepalive_cfg(sockfd, &cfg);
			ev.data.fd = sockfd;
			if(epoll_ctl(push_epollfd, EPOLL_CTL_ADD, sockfd, &ev)==-1){
				logger_p->printf("epoll_ctl failed 3 errno:%d\n",errno);
				syslog(LOG_ERR,"epoll_ctl failed 3 errno:%d\n",errno);
			}
		}
		close(servfd);
	}else{
		logger_p->printf("inital push server error\n");
		syslog(LOG_ERR,"inital push server error\n");
	}
}

void *push_request(void * arg){
	logger *logger_p=((common_data *)arg)->logger_p;
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
								search_ret=data_tree->search_try(id,(satellite **)&p,2);
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

void *timeout_server(void * arg){
	logger *logger_p=((common_data *)arg)->logger_p;
	rb_tree *data_tree=((common_data *)arg)->data_tree;
	int push_epollfd=((common_data *)arg)->push_epollfd;
	int epollfd=((common_data *)arg)->epollfd;
	link_list *link=((common_data *)arg)->link;
	link_list *callback_link=((common_data *)arg)->callback_link;

	struct epoll_event events[1];
	char buff[16];
	int len,sockfd;
	long id;
	user_data *p=0;
	message *msg_f=0;
	long current_time;

	for(;;){
		msg_f=link->pop_message();
		if(!msg_f){
			if(epoll_wait(epollfd, events, 1, TIME_OUT)<1){
				continue;
			}else{
				p=(user_data *)events[0].data.ptr;
				sockfd=p->fd;
				id=p->id;
				if(!(msg_f= link->take_out_message(id))){
					if((len=read(sockfd,buff,16))<=0){
						logger_p->printf("(id:%ld) broken link,unbinded client\n",id);
					}else{
						buff[len>15?15:len]=0;
						logger_p->printf("(id:%ld) receive data unexpected, len:%d buff:%s\n",id,len,buff);
					}
					p->mutex_unlock();
					data_tree->remove(id);
					continue;
				}
				if((len=read(sockfd,buff,16))<0){
					p->mutex_unlock();
					data_tree->remove(id);
					if(msg_f->callback_type==1){
						msg_f->push_success=false;
						callback_link->append_message(msg_f);
					}else{
						delete msg_f;
					}
					logger_p->printf("(id:%ld) broken link,unbinded client\n",id);
					continue;
				}
			}
		}else{
			current_time=getCurrentTime();
			if(msg_f->timestamp+TIME_OUT<=current_time){
				p=msg_f->p;
				sockfd=p->fd;
				id=p->id;
				if((len=recv(sockfd,buff,16,MSG_DONTWAIT))<=0){
					p->mutex_unlock();
					data_tree->remove(id);
					if(msg_f->callback_type==1){
						msg_f->push_success=false;
						callback_link->append_message(msg_f);
					}else{
						delete msg_f;
					}
					logger_p->printf("(id:%ld) ack timeout\n",id);
					continue;
				}
			}else{
				if(epoll_wait(epollfd, events, 1, msg_f->timestamp+TIME_OUT-current_time)<1){
					msg_f->p->mutex_unlock();
					data_tree->remove(msg_f->p->id);
					if(msg_f->callback_type==1){
						msg_f->push_success=false;
						callback_link->append_message(msg_f);
					}else{
						delete msg_f;
					}
					logger_p->printf("(id:%ld) ack timeout\n",id);
					continue;
				}else{
					p=(user_data *)events[0].data.ptr;
					sockfd=p->fd;
					id=p->id;
					if(id!=msg_f->p->id){
						link->append_message_to_head(msg_f);
						msg_f= link->take_out_message(id);
					}
					if((len=read(sockfd,buff,16))<=0){
						p->mutex_unlock();
						data_tree->remove(id);
						if(msg_f->callback_type==1){
							msg_f->push_success=false;
							callback_link->append_message(msg_f);
						}else{
							delete msg_f;
						}
						logger_p->printf("(id:%ld) broken link,unbinded client\n",id);
						continue;
					}
				}
			}
		}
		if(len==3 && memcmp(buff,"200",3)==0){//ack
			logger_p->printf("(id:%ld) receive ack , push success\n",id);
			if(msg_f->callback_type==1){
				msg_f->push_success=true;
				callback_link->append_message(msg_f);
			}else{
				delete msg_f;
			}
			message *message_p=p->pop_message();
			if(message_p){//待发送信息
				logger_p->printf("(id:%ld) send append message :%s\n",id,message_p->content);
				if(send(p->fd,message_p->content,strlen(message_p->content),MSG_NOSIGNAL)<0){ //发送失败，连接已失效
					p->mutex_unlock();
					data_tree->remove(id);
					delete message_p;
					logger_p->printf("(id:%ld) broken link , push failed\n",id);
				}else{
					link->append_message(message_p);
					logger_p->printf("(id:%ld) push success,waiting ack...\n",id);
				}
			}else{
				p->mutex_unlock();
				logger_p->printf("(id:%ld) push mission over\n",id);
			}
		}else{
			p->mutex_unlock();
			data_tree->remove(id);
			if(msg_f->callback_type==1){
				msg_f->push_success=false;
				callback_link->append_message(msg_f);
			}else{
				delete msg_f;
			}
			logger_p->printf("(id:%ld) verify ack failed\n",id);
		}
	}
}


void *callback_server(void * arg){
	logger *logger_p=((common_data *)arg)->logger_p;
	link_list *callback_link=((common_data *)arg)->callback_link;
	link_list *callback_write_link=((common_data *)arg)->callback_write_link;
	int callback_epollfd=((common_data *)arg)->callback_epollfd;
	message *msg_f=0;
	char domains[64];
	char port[8];
	int fd;
	struct epoll_event ev;
	ev.events = EPOLLOUT;

	/*
	正则表达式支持的回调格式：
	pm[1]:  以http://开头，可以缺省
	pm[2]:  不包含:和/的一个1-63字长的字符串，作为domain
	pm[3]:  以:开头，后跟1-5个数字的端口，可以缺省
	pm[4]:  以/开头，后跟1-4个不包含"/n"的0-128个字符串，可以缺省
	*/
	char pattern[]="^(http://)?([^:/]{1,63})(:[0-9]{1,5})?(/(.{0,128}){1,4})?$";
	const size_t n = 5;
	regmatch_t pm[5];
	regex_t reg;
	regcomp(&reg,pattern,REG_EXTENDED);

	struct addrinfo *aip;
	struct addrinfo hint;
	memset(&hint,0,sizeof(hint));
	hint.ai_flags=AI_CANONNAME;
	hint.ai_socktype=SOCK_STREAM;
	hint.ai_canonname=NULL;
	hint.ai_addr=NULL;
	hint.ai_next=NULL;

	for(;;){
		msg_f=callback_link->pop_message_wait();
		if(msg_f->callback_type==1 && strlen(msg_f->callback_url)<512){
			if(regexec(&reg,msg_f->callback_url,n,pm,0)==0){//匹配成功
				memset(domains,0,64);
				strncpy(domains,msg_f->callback_url+pm[2].rm_so, (size_t) pm[2].rm_eo-pm[2].rm_so);
				if(pm[3].rm_so!=-1){
					memset(port,0,8);
					strncpy(port,msg_f->callback_url+pm[3].rm_so+1, (size_t) pm[3].rm_eo-pm[3].rm_so-1);
				}else{
					strcpy(port,"80");
				}
				if(getaddrinfo(domains,port,&hint,&aip)==0){
					if((fd=socket(AF_INET,SOCK_STREAM,0))>=0){
						delete []msg_f->content;
						msg_f->content=new char[640]();
						if(pm[4].rm_so!=-1){
							sprintf(msg_f->content,"GET %.*s?push_result=%d&push_id=%d&client_id=%ld HTTP/1.1\r\nConnection: close\r\nHost: %s\r\n\r\n",
								(int) pm[4].rm_eo-pm[4].rm_so,msg_f->callback_url+pm[4].rm_so,msg_f->push_success?0:1,msg_f->push_id,msg_f->id,domains);
						}else{
							sprintf(msg_f->content,"GET %s?push_result=%d&push_id=%d&client_id=%ld HTTP/1.1\r\nConnection: close\r\nHost: %s\r\n\r\n","/",msg_f->push_success?0:1,msg_f->push_id,msg_f->id,domains);
						}
						fcntl(fd,F_SETFL,O_NONBLOCK);
						if(connect(fd,aip->ai_addr,aip->ai_addrlen)==0){//成功建立连接
							logger_p->printf("send callback message:\n=================================\n%s\n=================================\n",msg_f->content);
							send(fd,msg_f->content,strlen(msg_f->content),MSG_NOSIGNAL);
							close(fd);
							delete msg_f;
						}else{
							if(errno==EINPROGRESS){  //正在建立连接。。。
								msg_f->callback_fd=fd;
								ev.data.ptr=msg_f;   //ev.data是一个union，所有字段共享一个内存，只能赋一个值
								if (epoll_ctl(callback_epollfd, EPOLL_CTL_ADD, fd, &ev) == 0) {
									callback_write_link->append_message(msg_f);
								}else{
									logger_p->printf("(id:%ld) epoll_ctl failed 4 errno:%d\n",msg_f->id,errno);
									syslog(LOG_ERR,"(id:%ld) epoll_ctl failed 4 errno:%d\n",msg_f->id,errno);
									close(fd);
									delete msg_f;
								}
							}else{
								logger_p->printf("initTcpClient failed\n");
								syslog(LOG_ERR,"initTcpClient failed\n");
								close(fd);
								delete msg_f;
							}
						}
					}
				}
			}else{
				logger_p->printf("regex failed\n");
			}
		}else{
			logger_p->printf("callback param invalid :  callback_type:%d  strlen(callback_url):%d\n", msg_f->callback_type,strlen(msg_f->callback_url));
		}
	}
	regfree(&reg);
}


/*
负责向回调套接字中写入数据的线程
*/
void *callback_write_server(void * arg){
	int callback_epollfd=((common_data *)arg)->callback_epollfd;
	link_list *callback_write_link=((common_data *)arg)->callback_write_link;
	logger *logger_p=((common_data *)arg)->logger_p;
	struct epoll_event events[1];
	message *msg=0;
	int fd;
	long current_time;
	
	for(;;){
		msg=callback_write_link->pop_message_wait();
		current_time=getCurrentTime();
		if(msg->timestamp+CALLBACK_TIME_OUT<=current_time){
			fd=msg->callback_fd;
			logger_p->printf("send callback message:\n=================================\n%s\n=================================\n",msg->content);
			send(fd,msg->content,strlen(msg->content),MSG_NOSIGNAL);
			close(fd);
			delete msg;
			continue;
		}
		if(epoll_wait(callback_epollfd, events, 1, msg->timestamp+CALLBACK_TIME_OUT-current_time)<1){
			logger_p->printf("(id:%ld) send callback failed\n",msg->id);
			close(msg->callback_fd);
			delete msg;
		}else{
			if(msg!=events[0].data.ptr){
				callback_write_link->append_message_to_head(msg);
				msg=(message *)events[0].data.ptr;
				callback_write_link->take_out_message(msg);
			}
			fd=msg->callback_fd;
			logger_p->printf("send callback message:\n=================================\n%s\n=================================\n",msg->content);
			send(fd,msg->content,strlen(msg->content),MSG_NOSIGNAL);
			close(fd);
			delete msg;
		}
	}
}



int main(int argc,char *argv[]){
	daemonize("push_server");

	pthread_t tid_bind_tcp,tid_bind_request,tid_push_tcp,tid_push_request,tid_timeout,tid_callback,tid_callback_write;
	rb_tree data_tree;
	link_list link;
	link_list callback_link;
	link_list callback_write_link;
	int epollfd=epoll_create1(0);
	int bind_epollfd=epoll_create1(0);
	int push_epollfd=epoll_create1(0);
	int callback_epollfd=epoll_create1(0);
	logger logger(LOG_PATH);

	common_data c_data={&link,&data_tree,&callback_link,&callback_write_link,&logger,epollfd,bind_epollfd,push_epollfd,callback_epollfd};

	if(pthread_create(&tid_bind_tcp,NULL,bind_tcp_link,&c_data)==0){
		logger.printf("create bind link server thread success\n");
	}else{
		logger.printf("create bind link server thread failed\n");
		syslog(LOG_ERR,"create bind link server thread failed\n");
	}

	if(pthread_create(&tid_bind_request,NULL,bind_request,&c_data)==0){
		logger.printf("create bind request server thread success\n");
	}else{
		logger.printf("create bind request server thread failed\n");
		syslog(LOG_ERR,"create bind request server thread failed\n");
	}

	if(pthread_create(&tid_push_tcp,NULL,push_tcp_link,&c_data)==0){
		logger.printf("create push link server thread success\n");
	}else{
		logger.printf("create push link server thread failed\n");
		syslog(LOG_ERR,"create push link server thread failed\n");
	}

	if(pthread_create(&tid_push_request,NULL,push_request,&c_data)==0){
		logger.printf("create push request server thread success\n");
	}else{
		logger.printf("create push request server thread failed\n");
		syslog(LOG_ERR,"create push request server thread failed\n");
	}

	if(pthread_create(&tid_timeout,NULL,timeout_server,&c_data)==0){
		logger.printf("create timeout server thread success\n");
	}else{
		logger.printf("create timeout server thread failed\n");
		syslog(LOG_ERR,"create timeout server thread failed\n");
	}

	if(pthread_create(&tid_callback,NULL,callback_server,&c_data)==0){
		logger.printf("create callback server thread success\n");
	}else{
		logger.printf("create callback server thread failed\n");
		syslog(LOG_ERR,"create callback server thread failed\n");
	}

	if(pthread_create(&tid_callback_write,NULL,callback_write_server,&c_data)==0){
		logger.printf("create callback write server thread success\n");
	}else{
		logger.printf("create callback write server thread failed\n");
		syslog(LOG_ERR,"create callback write server thread failed\n");
	}

	logger.flush();

	pthread_join(tid_bind_tcp,NULL);
	pthread_join(tid_bind_request,NULL);
	pthread_join(tid_push_tcp,NULL);
	pthread_join(tid_push_request,NULL);
	pthread_join(tid_timeout,NULL);
	pthread_join(tid_callback,NULL);
	pthread_join(tid_callback_write,NULL);
}