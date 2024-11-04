#ifndef LST_TIMER
#define LST_TIMER

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>

#include <time.h>
#include "../log/log.h"

class util_timer;                       /* 前置声明 */

/* 客户端连接类 */
struct client_data {
    sockaddr_in address;                /* 客户端地址 */
    int sockfd;                         /* 客户端socket文件描述符 */
    util_timer *timer;                  /* 指向定时器的指针 */
};

/* 定时器类 */
class util_timer {
public:
    util_timer() : prev(NULL), next(NULL) {}

    time_t expire;                      /* 定时器过期时间 */
    
    void (* cb_func)(client_data *);    /* 回调函数 */
    client_data *user_data;             /* 客户端数据 */
    util_timer *prev;                   /* 双向链表指针 */
    util_timer *next;                   
};

/* 基于双向链表的定时器, 链表节点按到期时间升序排序 */
class sort_timer_lst {
public:
    sort_timer_lst();
    ~sort_timer_lst();

    void add_timer(util_timer *timer);      /* 添加定时器 */
    void adjust_timer(util_timer *timer);   /* 调整定时器位置 */
    void del_timer(util_timer *timer);      /* 删除定时器 */
    void tick();                            /* 处理到期的定时器 */

private:
    /* 私有函数，将定时器timer插入到链表中lst_head指向的定时器后面 */
    void add_timer(util_timer *timer, util_timer *lst_head);

    util_timer *head;                       /* 双向链表头指针 */
    util_timer *tail;                       /* 双向链表尾指针 */
};

class Utils {
public:
    Utils() {}
    ~Utils() {}

    void init(int timeslot);

    /* 文件描述符设置非阻塞 */
    int setnonblocking(int fd);             

    /* 将文件描述符（如套接字）注册到epoll内核事件表中，并配置相应的事件类型和选项 */
    void addfd(int epollfd, int fd, bool one_shot, int TRIGMode);

    /* 信号处理函数 */
    static void sig_handler(int sig);

    /* 设置指定信号的处理函数 */
    void addsig(int sig, void(handler)(int), bool restart = true);

    /* 处理定时任务，并重新设置定时器，以确保定时信号（SIGALRM）能够持续触发 */
    void timer_handler();

    /* 向客户端发送错误信息，并关闭相应的连接 */
    void show_error(int connfd, const char *info);

public:
    static int *u_pipefd;                   /* 用于存储管道的文件描述符数组，通常包含两个文件描述符：读端 (u_pipefd[0]) 和写端 (u_pipefd[1]) */
    static int u_epollfd;                   /* 用于存储 epoll 实例的文件描述符 */

    sort_timer_lst m_timer_lst;             /* 定时器链表 */
    int m_TIMESLOT;                         /* 定时器的时间间隔 */
};

void cb_func(client_data *user_data);       /* 回调函数，用于处理定时器过期时的操作 */

#endif
