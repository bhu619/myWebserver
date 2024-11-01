/*
    “#ifndef” 是 C、C++ 等编程语言中的预编译指令，全称为 “if not defined”，即 “如果未定义”。
    它通常用于防止头文件被重复包含。
*/
#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <cstdlib>
#include <cstdio>
#include <getopt.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <cassert>
#include <sys/epoll.h>

#include "./threadpool/threadpool.h"
#include "./http/http_conn.h"

const int MAX_FD = 65536;           /* 最大文件描述符 */
const int MAX_EVENT_NUMBER = 10000; /* 最大事件数 */
const int TIMESLOT = 5;             /* 最小超时单位 */

class WebServer
{
public:
    WebServer();
    ~WebServer();

    void init(int port , string user, string passWord, string databaseName,
              int log_write , int opt_linger, int trigmode, int sql_num,
              int thread_num, int close_log, int actor_model);

    void thread_pool();
    void sql_pool();
    void log_write();
    void trig_mode();
    void eventListen();
    void eventLoop();
    void timer(int connfd, struct sockaddr_in client_address);
    void adjust_timer(util_timer *timer);
    void deal_timer(util_timer *timer, int sockfd);
    bool dealclientdata();
    bool dealwithsignal(bool& timeout, bool& stop_server);
    void dealwithread(int sockfd);
    void dealwithwrite(int sockfd);

public:
    
    int m_port;                     /* 服务器端口号 */
    char *m_root;                   /* 根目录 */
    int m_log_write;                /* 同步 / 异步 日志 */
    int m_close_log;                /* 开启日志 */
    int m_actormodel;               /* 线程池并发模式 */

    int m_pipefd[2];                /* 用于进程间通信的管道文件描述符 */
    int m_epollfd;                  /* epoll 文件描述符 */
    http_conn *users;

    connection_pool *m_connPool;    /* 数据库连接池 */
    string m_user;                  /* 登陆数据库用户名 */
    string m_passWord;              /* 登陆数据库密码 */
    string m_databaseName;          /* 使用数据库名 */
    int m_sql_num;                  /* 数据库连接池大小, 默认为8 */

    threadpool<http_conn> *m_pool;  /* 线程池 */
    int m_thread_num;               /* 线程池线程数目，默认为8 */

    //epoll_event相关
    epoll_event events[MAX_EVENT_NUMBER];

    int m_listenfd;                    /* 监听套接字文件描述符 */
    int m_OPT_LINGER;
    int m_TRIGMode;
    int m_LISTENTrigmode;
    int m_CONNTrigmode;

    //定时器相关
    client_data *users_timer;
    Utils utils;
};
#endif
