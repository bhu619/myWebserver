#include "lst_timer.h"
#include "../http/http_conn.h"

sort_timer_lst::sort_timer_lst() {
    head = NULL;
    tail = NULL;
}

sort_timer_lst::~sort_timer_lst() {

    util_timer *tmp = head;

    while (tmp) {
        head = tmp->next;
        delete tmp;
        tmp = head;
    }
}

void sort_timer_lst::add_timer(util_timer *timer) {

    if (!timer) { return; }

    /* 如果链表为空 */
    if (!head) {
        head = tail = timer;
        return;
    }

    /* 插入节点的到期时间小于头节点 */
    if (timer->expire < head->expire) {
        timer->next = head;
        head->prev = timer;
        head = timer;
        return;
    }

    add_timer(timer, head);
}

void sort_timer_lst::adjust_timer(util_timer *timer) {

    if (!timer) { return; }

    util_timer *tmp = timer->next;

    /* 如果 (不存在下一个定时器 || 当前定时器到期时间仍然小于下一个定时器), 不需要调整位置 */
    if (!tmp || (timer->expire < tmp->expire)) {
        return;
    }

    /* 需要调整位置 */
    if (timer == head) {
        head = head->next;
        head->prev = NULL;
        timer->next = NULL;
        add_timer(timer, head);
    } else {
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
        add_timer(timer, timer->next);
    }
}

void sort_timer_lst::del_timer(util_timer *timer) {
    /* 
        从链表中删除指定的定时器。需要考虑五种情况：
            定时器为空
            定时器是唯一的节点。
            定时器是头节点。
            定时器是尾节点。
            定时器是中间节点。
    */

    if (!timer) { return; }

    if ((timer == head) && (timer == tail)) {
        delete timer;
        head = NULL;
        tail = NULL;
        return;
    }

    if (timer == head) {
        head = head->next;
        head->prev = NULL;
        delete timer;
        return;
    }

    if (timer == tail) {
        tail = tail->prev;
        tail->next = NULL;
        delete timer;
        return;
    }

    timer->prev->next = timer->next;
    timer->next->prev = timer->prev;
    delete timer;
}

void sort_timer_lst::tick() {

    if (!head) { return; }
    
    time_t cur = time(NULL);    /* 获取系统当前时间 */
    util_timer *tmp = head;

    while (tmp) {
        if (cur < tmp->expire) {    /* 如果当前时间小于定时器到期时间，直接终止遍历 */
            break;
        }

        tmp->cb_func(tmp->user_data);

        head = tmp->next;
        if (head) {
            head->prev = NULL;
        }

        delete tmp;
        tmp = head;
    }
}

void sort_timer_lst::add_timer(util_timer *timer, util_timer *lst_head)
{
    util_timer *prev = lst_head;
    util_timer *tmp = prev->next;
    while (tmp) {
        /* 找到了插入位置 */
        if (timer->expire < tmp->expire) {
            prev->next = timer;
            timer->next = tmp;
            tmp->prev = timer;
            timer->prev = prev;
            break;
        }
        prev = tmp;
        tmp = tmp->next;
    }

    /* 插入链表尾部 */
    if (!tmp) {
        prev->next = timer;
        timer->prev = prev;
        timer->next = NULL;
        tail = timer;
    }
}

/* 初始化定时信号的时间间隔 */
void Utils::init(int timeslot) {
    m_TIMESLOT = timeslot;
}

/* 文件描述符设置非阻塞 */
int Utils::setnonblocking(int fd) {
    int old_option = fcntl(fd, F_GETFL);        /* 获取到文件描述符fd当前的文件状态标志 */

    if (old_option == -1) {
        perror("fcntl F_GETFL failed");
        return -1;
    }

    int new_option = old_option | O_NONBLOCK;

    if (fcntl(fd, F_SETFL, new_option) == -1) {
        perror("fcntl F_SETFL failed");
        return -1;
    }

    return old_option;
}

/* 将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT */
void Utils::addfd(int epollfd, int fd, bool one_shot, int TRIGMode)
{
    epoll_event event;                          /* 用于描述要注册的事件 */
    event.data.fd = fd;

    if (1 == TRIGMode) {
        /* 事件类型包括：可读事件/边缘触发模式/检测远程关闭连接或半关闭状态 */
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    } else {
        /* 可读事件/检测远程关闭连接或半关闭状态 */
        event.events = EPOLLIN | EPOLLRDHUP;
    }
        
    if (one_shot) {
        /* 为事件添加EPOLLONESHOT选项 */
        event.events |= EPOLLONESHOT;
    }

    /* 将配置好的事件注册到指定的epoll实例epollfd中 */
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event) == -1) {
        std::cerr << "epoll_ctl add failed: " << strerror(errno) << std::endl;
        close(fd);                              /* 关闭文件描述符以防资源泄漏 */
        return;
    }
    
    if (setnonblocking(fd) == -1) {
        std::cerr << "setnonblocking failed: " << strerror(errno) << std::endl;
        close(fd);
        return;
    }
}

/* 信号处理函数 */
void Utils::sig_handler(int sig) {

    int save_errno = errno;                     /* 保证函数的可重入性，保留原来的errno */
    int msg = sig;
    write(u_pipefd[1], (char *)&msg, 1);        /* 将send替换为write, write异步信号安全 */ 
    errno = save_errno;
}

/* 设置指定信号的处理函数 */
void Utils::addsig(int sig, void(handler)(int), bool restart) {
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;                    /* 指定信号处理函数 */

    if (restart) {
        sa.sa_flags |= SA_RESTART;              /* 设置信号处理函数在被信号中断后自动重启被中断的系统调用 */
    }
        
    sigfillset(&sa.sa_mask);                    /* 屏蔽所有信号，防止信号处理函数被中断 */

    if (sigaction(sig, &sa, NULL) == -1) {
        std::cerr << "sigaction failed for signal " << sig << ": " << strerror(errno) << std::endl;
        exit(-1);
    }
}

/* 处理定时任务，并重新设置定时器，以确保定时信号（SIGALRM）能够持续触发 */
void Utils::timer_handler() {
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGALRM);
    sigprocmask(SIG_BLOCK, &mask, NULL);

    m_timer_lst.tick();                         /* 调用定时器链表 m_timer_lst 的 tick 方法，处理所有到期的定时器 */
    alarm(m_TIMESLOT);                          /* 使用 alarm 函数重新设置定时器，使得下一个 SIGALRM 信号在 m_TIMESLOT 秒后触发 */

    sigprocmask(SIG_UNBLOCK, &mask, NULL);
}

/* 向客户端发送错误信息，并关闭相应的连接 */
void Utils::show_error(int connfd, const char *info)
{
    ssize_t bytes_sent = send(connfd, info, strlen(info), 0);

    if (bytes_sent == -1) {
        std::cerr << "send failed: " << strerror(errno) << std::endl;
    }

    shutdown(connfd, SHUT_RDWR);                /* 同时关闭连接的读和写两个方向 */
    close(connfd);
}

int *Utils::u_pipefd = 0;
int Utils::u_epollfd = 0;

class Utils;                                    /* 前置声明 */
/* 定时器回调函数 */
void cb_func(client_data *user_data) {

    if (!user_data) {
        std::cerr << "cb_func received null user_data" << std::endl;
        return;
    }

    /* 从 epoll 中删除文件描述符 */
    if (epoll_ctl(Utils::u_epollfd, EPOLL_CTL_DEL, user_data->sockfd, nullptr) == -1) {
        std::cerr << "epoll_ctl DEL failed: " << strerror(errno) << std::endl;
    }

    assert(user_data);

    if (close(user_data->sockfd) == -1) {
        std::cerr << "close failed: " << strerror(errno) << std::endl;
    }

    http_conn::m_user_count--;                  /* 减少用户计数 */
}
