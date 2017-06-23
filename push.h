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


#define SERVER_PORT "3889"
#define MAX_MESSAGE_SIZE 1024
#define READ_TIME_OUT 1000



class Log{
public:
	Log(const char *path);
	~Log();
	void printf(const char *format,...);
	void flush();
private:
	FILE * file;

	char buff[32];
	time_t t;
	struct tm *tmp;
};


class client : public satellite {
public:
    client(long id,int fd);
    ~client();

    long id;
    int fd;
};

class list_item {
public:
	list_item(long timestamp,long data);
	
	long timestamp;
	long data;
	list_item *prev;
    list_item *next;
};

class linked_list {
public:
    linked_list();
    ~linked_list();
	void append(long data);
    void append_to_head(list_item *item);
    list_item *pop();
    list_item *get(long data);
    
private:
	list_item *item_f;
    list_item *item_l;
    pthread_mutex_t *mutex;
};

struct common_data{
	rb_tree *data;
	Log *logger;
	int epollfd_socket;
    int epollfd_socket_et;
    int epollfd_client;
    int epollfd_client_et;
    linked_list *list_socket;
    linked_list *list_client;
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

unsigned long ntohl64(char *buff);