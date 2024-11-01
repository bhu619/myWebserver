#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>
#include <memory>
#include "../lock/locker.h"
#include "../CGImysql/sql_connection_pool.h"
#include "../log/log.h"     /* 日志 */

/* 该线程池类是一个模板类，可以接受类型参数 T，通常用于处理 HTTP 连接的 http_conn 对象 */
template <typename T>
class threadpool
{
public:
    /* thread_number是线程池中线程的数量，max_requests是请求队列中最多允许的、等待处理的请求的数量 */
    threadpool(int actor_model, connection_pool *connPool, int thread_number = 8, int max_request = 10000);
    ~threadpool();
    bool append(T *request, int state);
    bool append_p(T *request);

private:
    /* 工作线程运行的函数，它不断从工作队列中取出任务并执行之 */
    static void *worker(void *arg);
    void run();

private:
    int m_thread_number;            /* 线程池中的线程数 */
    int m_max_requests;             /* 请求队列中允许的最大请求数 */
    pthread_t *m_threads;           /* 线程标识符 */
    std::list<T *> m_workqueue;     /* 请求队列 */
    locker m_queuelocker;           /* 保护请求队列的互斥锁 */
    sem m_queuestat;                /* 是否有任务需要处理 */
    connection_pool *m_connPool;    /* 数据库连接池 */
    int m_actor_model;              /* 并发模型切换 */
    bool m_stop;                    /* 用于通知线程退出 */
};

/* 线程池构造函数 */
template <typename T>
threadpool<T>::threadpool( int actor_model, connection_pool *connPool, int thread_number, int max_requests) 
                                : m_actor_model(actor_model),
                                    m_thread_number(thread_number), 
                                    m_max_requests(max_requests), 
                                    m_threads(NULL),
                                    m_connPool(connPool),
                                    m_stop(false)
{
    if (thread_number <= 0 || max_requests <= 0)
        throw std::exception();

    /* 使用 unique_ptr 管理线程数组 */
    std::unique_ptr<pthread_t[]> threads(new pthread_t[thread_number]);

    pthread_attr_t attr;                                            /* 线程属性 */
    pthread_attr_init(&attr);                                       /* 初始化 attr 为默认值 */
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);    /* 设置线程为分离状态 */

    for (int i = 0; i < thread_number; ++i)
    {
        /* 如果线程创建失败 */
        if (pthread_create(&threads[i], &attr, worker, this) != 0)
        {
            pthread_attr_destroy(&attr);                            /* 销毁线程属性对象 */
            
            for (int j = 0; j < i; ++j)
            {
                pthread_cancel(threads[j]);                         /* 取消已经创建的线程 */
            }
            throw std::exception();
        }
    }

    pthread_attr_destroy(&attr);

    /* 成功创建所有线程，释放控制权给成员变量 */ 
    m_threads = threads.release();
}

/* 析构函数 */
template <typename T>
threadpool<T>::~threadpool()
{
    
    m_stop = true;              /* 通知所有线程停止 */

    locker_guard lock(m_queuelocker);
    m_workqueue.clear();        /* 清空请求队列，防止线程在析构后继续取任务 */

    delete[] m_threads;         /* 释放线程标识符数组内存 */

    /* 当 locker_guard 对象 lock 离开作用域时（即析构函数中的作用域结束时），locker_guard 的析构函数被调用，它会自动在 m_queuelocker 上调用 unlock() 方法，解锁互斥锁 */
}

/* 往工作队列中添加一个任务请求 request，并设置任务的状态 state */
template <typename T>
bool threadpool<T>::append(T *request, int state)
{
    /* 对工作队列的访问必须是线程安全的 */
    locker_guard lock(m_queuelocker);

    /* 如果请求队列已满 */
    if (m_workqueue.size() >= m_max_requests)
    {
        std::cerr << "Request queue is full, failed to add task."  << std::endl;
        LOG_ERROR("Request queue is full, failed to add task.");
        return false;
    }

    request->m_state = state;
    m_workqueue.push_back(request);
    
    m_queuestat.post(); /* 信号量的P操作，通知有新任务到来，等待中的工作线程将开始处理这个请求 */

    return true;
}

/* 往工作队列中添加一个任务请求 request，但不设置任务的状态 state */
template <typename T>
bool threadpool<T>::append_p(T *request)
{
    locker_guard lock(m_queuelocker);

    if (m_workqueue.size() >= m_max_requests)
    {
        std::cerr << "Request queue is full, failed to add task."  << std::endl;
        LOG_ERROR("Request queue is full, failed to add task.");
        return false;
    }

    m_workqueue.push_back(request);
    
    m_queuestat.post();

    return true;
}

/* 工作线程的入口函数 */
template <typename T>
void *threadpool<T>::worker(void *arg)
{
    threadpool *pool = (threadpool *)arg;   /* 将传入的 void* 参数转换为 threadpool<T>* 类型 */
    pool->run();                            /* 调用线程池的 run() 方法，处理任务 */
    return pool;                            /* 返回 pool 指针 */
}

/* 工作线程的核心任务处理逻辑 */
template <typename T>
void threadpool<T>::run() {

    /* 当工作线程没有停止 */
    while (!m_stop)
    {
        m_queuestat.wait();                 /* 等待信号量，确保有任务可执行，在信号量为零时，线程会阻塞 */

        m_queuelocker.lock();

        if (m_workqueue.empty())            /* 如果任务对列为空，解锁并继续等待 */
        {
            m_queuelocker.unlock();
            continue;
        }

        T *request = m_workqueue.front();   /* 从队列中取出任务 */
        m_workqueue.pop_front();

        m_queuelocker.unlock();             /* 这里使用手动解锁，是为了及时解锁，减小锁的持有时间，提升线程池的并发性能 */

        if (!request) {                     /* 如果请求为空，则跳过 */
            continue;
        }

        /* 根据actor模型处理不同的请求 */
        if (1 == m_actor_model) {           
            if (0 == request->m_state) {    /* 读取请求 */
                if (request->read_once()) {
                    request->improv = 1;

                    std::shared_ptr<MYSQL> mysql_ptr(request->mysql, [](MYSQL* ptr) { 
                        mysql_close(ptr);   /* 使用自定义删除器确保 mysql 连接正确释放 */
                    });

                    /* 使用 connectionRAII 来管理连 */
                    connectionRAII mysqlcon(mysql_ptr, m_connPool);
                    request->process();     /* 处理请求 */
                }
                else {
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            } else {                         /* 写入请求 */
                if (request->write()) {
                    request->improv = 1;
                } else {
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            }
        } else {
            std::shared_ptr<MYSQL> mysql_ptr(request->mysql, [](MYSQL* ptr) {   /* 使用 shared_ptr 包装 mysql 连接 */
                mysql_close(ptr);
            });

            /* 使用 connectionRAII 来管理连 */
            connectionRAII mysqlcon(mysql_ptr, m_connPool);
            request->process();
        }
    }
}

#endif
