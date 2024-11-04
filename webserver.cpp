#include "webserver.h"

WebServer::WebServer()
{
    //http_conn类对象
    users = new http_conn[MAX_FD];

    //root文件夹路径
    char server_path[200];
    getcwd(server_path, 200);
    char root[6] = "/root";
    m_root = (char *)malloc(strlen(server_path) + strlen(root) + 1);
    strcpy(m_root, server_path);
    strcat(m_root, root);

    //定时器
    users_timer = new client_data[MAX_FD];
}

WebServer::~WebServer()
{
    /* 释放内存和资源 */
    close(m_epollfd);
    close(m_listenfd);
    close(m_pipefd[1]);
    close(m_pipefd[0]);
    delete[] users;
    delete[] users_timer;
    delete m_pool;
}

/* init：初始化服务器，包括端口号、数据库信息、线程池参数等 */
void WebServer::init(int port, string user, string passWord, string databaseName, int log_write, 
                     int opt_linger, int trigmode, int sql_num, int thread_num, int close_log, int actor_model)
{
    m_port = port;
    m_user = user;
    m_passWord = passWord;
    m_databaseName = databaseName;
    m_sql_num = sql_num;
    m_thread_num = thread_num;
    m_log_write = log_write;
    m_OPT_LINGER = opt_linger;
    m_TRIGMode = trigmode;
    m_close_log = close_log;
    m_actormodel = actor_model;
}

/* 配置触发模式, 设置服务器的监听套接字（m_LISTENTrigmode）和连接套接字（m_CONNTrigmode）的触发模式 */
void WebServer::trig_mode()
{
    
    if (0 == m_TRIGMode) {
        m_LISTENTrigmode = 0;   /* 监听套接字设置为 LT 模式 */
        m_CONNTrigmode = 0;     /* 连接套接字设置为 LT 模式 */
    }

    else if (1 == m_TRIGMode) {
        m_LISTENTrigmode = 0;   /* 监听套接字设置为 LT 模式 */
        m_CONNTrigmode = 1;     /* 连接套接字设置为 ET 模式 */
    }

    else if (2 == m_TRIGMode) {
        m_LISTENTrigmode = 1;   /* 监听套接字设置为 ET 模式 */
        m_CONNTrigmode = 0;     /* 连接套接字设置为 LT 模式 */
    }

    else if (3 == m_TRIGMode) {
        m_LISTENTrigmode = 1;   /* 监听套接字设置为 ET 模式 */
        m_CONNTrigmode = 1;     /* 连接套接字设置为 ET 模式 */
    }
}

/* 创建并初始化日志 */
void WebServer::log_write()
{
    if (0 == m_close_log)
    {
        // 日志文件路径和名称
        const char *log_file_name = "./ServerLog";

        // 日志缓冲区大小
        const int log_buffer_size = 2000;

        // 日志文件最大行数
        const int max_lines = 800000;

        // 阻塞队列大小（仅异步模式下使用）
        const int max_queue_size = (m_log_write == 1) ? 800 : 0;

        // 初始化日志系统
        bool init_status = Log::get_instance()->init(log_file_name, m_close_log, log_buffer_size, max_lines, max_queue_size);
        if (!init_status)
        {
            std::cerr << "Failed to initialize log system." << std::endl;
            exit(EXIT_FAILURE);
        }
    }
}

/* 创建并初始化数据库连接池 */
void WebServer::sql_pool()
{
    /* 初始化数据库连接池 */
    m_connPool = connection_pool::GetInstance();
    m_connPool->init("localhost", m_user, m_passWord, m_databaseName, 3306, m_sql_num, m_close_log);

    /* 初始化数据库读取表 */
    users->initmysql_result(m_connPool);
}

/* 创建并初始化服务器的线程池 */
void WebServer::thread_pool()
{
    /* 输入参数: 并发模型，数据库连接池，线程池线程数 */
    m_pool = new threadpool<http_conn>(m_actormodel, m_connPool, m_thread_num);
}

/* 事件监听 */
void WebServer::eventListen()
{
    m_listenfd = socket(PF_INET, SOCK_STREAM, 0);   /* 创建一个套接字，使用IPv4，面向连接的字节流服务 */
    assert(m_listenfd >= 0);                        /* 确保创建成功 */

    /* 优雅关闭连接 */
    if (0 == m_OPT_LINGER)              /* 立即关闭套接字 */
    {
        struct linger tmp = {0, 1};     /* 用于操作SO_LINGER选项的结构体 */
        setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    }
    else if (1 == m_OPT_LINGER)         /* 启用延迟关闭*/
    {
        struct linger tmp = {1, 1};
        setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));   /* 设置套接字选项，确保套接字在关闭时能优雅地处理未完成的连接。 */
    }

    int ret = 0;
    struct sockaddr_in address;                     /* TCP/IP 协议族结构体 */
    bzero(&address, sizeof(address));               /* 将 address 结构体的所有字节设置为 0，确保未使用的字段被清零 */
    address.sin_family = AF_INET;                   /* 地址族，IPv4 */
    address.sin_addr.s_addr = htonl(INADDR_ANY);    /* INADDR_ANY 表示监听所有可用的接口（IPv4地址） */
    address.sin_port = htons(m_port);               /* htons 将端口号的主机字节序转换为网络字节序 */

    int flag = 1;
    /* 
        设置的具体选项为地址重用。
        当设置了这个选项后，允许在相同的 IP 地址和端口上快速重用套接字，即使之前的连接还处于 TIME_WAIT 状态。
        这在服务器程序中很有用，特别是当服务器需要快速重启时，可以避免因为等待 TIME_WAIT 状态结束而导致的延迟。 
    */
    setsockopt(m_listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
    ret = bind(m_listenfd, (struct sockaddr *)&address, sizeof(address));   /* 绑定套接字到指定的地址和端口 */
    assert(ret >= 0);
    ret = listen(m_listenfd, 5);                    /* 监听队列长度为5 */   
    assert(ret >= 0);

    utils.init(TIMESLOT);                           /* 初始化定时器 */

    //epoll创建内核事件表
    epoll_event events[MAX_EVENT_NUMBER];           /* 事件数组，用于存储 epoll 检测到的事件 */
    m_epollfd = epoll_create(5);                    /* 创建一个 epoll 实例 */
    assert(m_epollfd != -1);

    utils.addfd(m_epollfd, m_listenfd, false, m_LISTENTrigmode);
    http_conn::m_epollfd = m_epollfd;               /* 将 epoll 文件描述符赋值给 http_conn 类的静态成员变量，用于处理 HTTP 连接 */

    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, m_pipefd);    /* 创建一对无名管道，用于在进程内部进行双向通信，常用于信号处理 */
    assert(ret != -1);
    utils.setnonblocking(m_pipefd[1]);              /* 将管道的写端设置为非阻塞模式 */
    utils.addfd(m_epollfd, m_pipefd[0], false, 0);  /* 将管道的读端添加到 epoll 实例中，用于监听信号 */

    utils.addsig(SIGPIPE, SIG_IGN);
    utils.addsig(SIGALRM, utils.sig_handler, false);
    utils.addsig(SIGTERM, utils.sig_handler, false);

    alarm(TIMESLOT);

    //工具类,信号和描述符基础操作
    Utils::u_pipefd = m_pipefd;
    Utils::u_epollfd = m_epollfd;
}

void WebServer::timer(int connfd, struct sockaddr_in client_address)
{
    users[connfd].init(connfd, client_address, m_root, m_CONNTrigmode, m_close_log, m_user, m_passWord, m_databaseName);

    //初始化client_data数据
    //创建定时器，设置回调函数和超时时间，绑定用户数据，将定时器添加到链表中
    users_timer[connfd].address = client_address;
    users_timer[connfd].sockfd = connfd;
    util_timer *timer = new util_timer;
    timer->user_data = &users_timer[connfd];
    timer->cb_func = cb_func;
    time_t cur = time(NULL);
    timer->expire = cur + 3 * TIMESLOT;
    users_timer[connfd].timer = timer;
    utils.m_timer_lst.add_timer(timer);
}

//若有数据传输，则将定时器往后延迟3个单位
//并对新的定时器在链表上的位置进行调整
void WebServer::adjust_timer(util_timer *timer)
{
    time_t cur = time(NULL);
    timer->expire = cur + 3 * TIMESLOT;
    utils.m_timer_lst.adjust_timer(timer);

    LOG_INFO("%s", "adjust timer once");
}

void WebServer::deal_timer(util_timer *timer, int sockfd)
{
    timer->cb_func(&users_timer[sockfd]);
    if (timer)
    {
        utils.m_timer_lst.del_timer(timer);
    }

    LOG_INFO("close fd %d", users_timer[sockfd].sockfd);
}

bool WebServer::dealclientdata()
{
    struct sockaddr_in client_address;
    socklen_t client_addrlength = sizeof(client_address);
    if (0 == m_LISTENTrigmode)
    {
        int connfd = accept(m_listenfd, (struct sockaddr *)&client_address, &client_addrlength);
        if (connfd < 0)
        {
            LOG_ERROR("%s:errno is:%d", "accept error", errno);
            return false;
        }
        if (http_conn::m_user_count >= MAX_FD)
        {
            utils.show_error(connfd, "Internal server busy");
            LOG_ERROR("%s", "Internal server busy");
            return false;
        }
        timer(connfd, client_address);
    }

    else
    {
        while (1)
        {
            int connfd = accept(m_listenfd, (struct sockaddr *)&client_address, &client_addrlength);
            if (connfd < 0)
            {
                LOG_ERROR("%s:errno is:%d", "accept error", errno);
                break;
            }
            if (http_conn::m_user_count >= MAX_FD)
            {
                utils.show_error(connfd, "Internal server busy");
                LOG_ERROR("%s", "Internal server busy");
                break;
            }
            timer(connfd, client_address);
        }
        return false;
    }
    return true;
}

bool WebServer::dealwithsignal(bool &timeout, bool &stop_server)
{
    int ret = 0;
    int sig;
    char signals[1024];
    ret = recv(m_pipefd[0], signals, sizeof(signals), 0);
    if (ret == -1)
    {
        return false;
    }
    else if (ret == 0)
    {
        return false;
    }
    else
    {
        for (int i = 0; i < ret; ++i)
        {
            switch (signals[i])
            {
            case SIGALRM:
            {
                timeout = true;
                break;
            }
            case SIGTERM:
            {
                stop_server = true;
                break;
            }
            }
        }
    }
    return true;
}

void WebServer::dealwithread(int sockfd)
{
    util_timer *timer = users_timer[sockfd].timer;

    //reactor
    if (1 == m_actormodel)
    {
        if (timer)
        {
            adjust_timer(timer);
        }

        //若监测到读事件，将该事件放入请求队列
        m_pool->append(users + sockfd, 0);

        while (true)
        {
            if (1 == users[sockfd].improv)
            {
                if (1 == users[sockfd].timer_flag)
                {
                    deal_timer(timer, sockfd);
                    users[sockfd].timer_flag = 0;
                }
                users[sockfd].improv = 0;
                break;
            }
        }
    }
    else
    {
        //proactor
        if (users[sockfd].read_once())
        {
            LOG_INFO("deal with the client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));

            //若监测到读事件，将该事件放入请求队列
            m_pool->append_p(users + sockfd);

            if (timer)
            {
                adjust_timer(timer);
            }
        }
        else
        {
            deal_timer(timer, sockfd);
        }
    }
}

void WebServer::dealwithwrite(int sockfd)
{
    util_timer *timer = users_timer[sockfd].timer;
    //reactor
    if (1 == m_actormodel)
    {
        if (timer)
        {
            adjust_timer(timer);
        }

        m_pool->append(users + sockfd, 1);

        while (true)
        {
            if (1 == users[sockfd].improv)
            {
                if (1 == users[sockfd].timer_flag)
                {
                    deal_timer(timer, sockfd);
                    users[sockfd].timer_flag = 0;
                }
                users[sockfd].improv = 0;
                break;
            }
        }
    }
    else
    {
        //proactor
        if (users[sockfd].write())
        {
            LOG_INFO("send data to the client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));

            if (timer)
            {
                adjust_timer(timer);
            }
        }
        else
        {
            deal_timer(timer, sockfd);
        }
    }
}

void WebServer::eventLoop()
{
    bool timeout = false;
    bool stop_server = false;

    while (!stop_server)
    {
        int number = epoll_wait(m_epollfd, events, MAX_EVENT_NUMBER, -1);
        if (number < 0 && errno != EINTR)
        {
            LOG_ERROR("%s", "epoll failure");
            break;
        }

        for (int i = 0; i < number; i++)
        {
            int sockfd = events[i].data.fd;

            //处理新到的客户连接
            if (sockfd == m_listenfd)
            {
                bool flag = dealclientdata();
                if (false == flag)
                    continue;
            }
            else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
            {
                //服务器端关闭连接，移除对应的定时器
                util_timer *timer = users_timer[sockfd].timer;
                deal_timer(timer, sockfd);
            }
            //处理信号
            else if ((sockfd == m_pipefd[0]) && (events[i].events & EPOLLIN))
            {
                bool flag = dealwithsignal(timeout, stop_server);
                if (false == flag)
                    LOG_ERROR("%s", "dealclientdata failure");
            }
            //处理客户连接上接收到的数据
            else if (events[i].events & EPOLLIN)
            {
                dealwithread(sockfd);
            }
            else if (events[i].events & EPOLLOUT)
            {
                dealwithwrite(sockfd);
            }
        }
        if (timeout)
        {
            utils.timer_handler();

            LOG_INFO("%s", "timer tick");

            timeout = false;
        }
    }
}
