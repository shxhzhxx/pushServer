#include "rb_tree.h"
#include <pthread.h>
#include <sys/epoll.h>
#include <regex.h>
#include <stdarg.h>
#include <syslog.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/resource.h>

#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <ctype.h>
#include <sys/time.h>
#include <syslog.h>
#include <fcntl.h>
#include <math.h>
#include <ctype.h>
#include <float.h>



#define SERVER_IP_ADDRESS "192.168.2.109"
#define BIND_SERVER_PORT "3889"
#define PUSH_SERVER_PORT "3899"
#define MAX_EVENTS 10
#define TIME_OUT 5000
#define CALLBACK_TIME_OUT 10000
#define MAX_MESSAGE_SIZE 512
#define LOG_PATH "/home/shxhzhxx/tcp_push/push_log.log"



class user_data;

class logger{
public:
	logger(const char *path);
	~logger();
	void printf(const char *format,...);
	void flush();
private:
	FILE * file;

	char buff[32];
	time_t t;
	struct tm *tmp;
};


class message {
public:
	message(const char *_content,int _callback_type,long _id,int _push_id=0,const char *_callback_url=NULL);
	message(const char *_content,int _callback_type,user_data *_p,int _push_id=0,const char *_callback_url=NULL);
	~message();
	
	int callback_type;
	int push_id;
	char *callback_url;
	bool push_success;
	char *content;
	message *next;

	long timestamp;
	user_data *p;
	long id;
	int callback_fd;
	message *prev;
private:
	void initial(const char *_content,const char *_callback_url);
};


class user_data : public satellite {
public:
    user_data(long _id,int _fd = 0);
    ~user_data();
    void append_message(message *msg);
    message *pop_message();

    long id;
    int device_type;
    char *device_token;
    int fd;
private:
    pthread_mutex_t *msg_mutex;
    message *msg_f;
    message *msg_l;
};


class link_list {
public:
    link_list();
    ~link_list();
    void append_message_to_head(message *msg);
	void append_message(message *msg);
    message *pop_message();
    message *pop_message_wait();
    message *take_out_message(long id);
    message *take_out_message(message *msg);
    
private:
	message *msg_f;
    message *msg_l;
    pthread_mutex_t *mutex;
    pthread_cond_t *cond;
};

struct common_data{
	link_list *link;
	rb_tree *data_tree;
	link_list *callback_link;
	link_list *callback_write_link;
	logger *logger_p;
	int epollfd;
	int bind_epollfd;
	int push_epollfd;
	int callback_epollfd;
};

struct KeepConfig {
    /** The time (in seconds) the connection needs to remain 
     * idle before TCP starts sending keepalive probes (TCP_KEEPIDLE socket option)
     */
    int keepidle;
    /** The maximum number of keepalive probes TCP should 
     * send before dropping the connection. (TCP_KEEPCNT socket option)
     */
    int keepcnt;

    /** The time (in seconds) between individual keepalive probes.
     *  (TCP_KEEPINTVL socket option)
     */
    int keepintvl;
};

long getCurrentTime();

/**
* enable TCP keepalive on the socket
* @param fd file descriptor
* @return 0 on success -1 on failure
*/
int set_tcp_keepalive(int sockfd);

/** Set the keepalive options on the socket
* This also enables TCP keepalive on the socket
*
* @param fd file descriptor
* @param fd file descriptor
* @return 0 on success -1 on failure
*/
int set_tcp_keepalive_cfg(int sockfd, const struct KeepConfig *cfg);

int initTcpServer(const char * port);

int initTcpClient(const char *ip,const char *port);

int daemonize(const char *cmd);