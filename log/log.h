#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <iostream>
#include <string>
#include <stdarg.h>
#include <pthread.h>
#include "block_queue.h"
#include <exception>  // 用于捕获和处理异常

using namespace std;

class Log
{
public:
    /* C++11以后,使用局部变量懒汉不用加锁 */
    static Log *get_instance()
    {
        static Log instance;
        return &instance;
    }

    /* 线程入口函数，用于异步写入日志 */
    static void *flush_log_thread(void *args)
    {
        try
            {
                // 获取日志单例并执行异步写入
                Log::get_instance()->async_write_log();
            }
        catch (const std::exception &e)
            {
                std::cerr << "Exception in flush_log_thread: " << e.what() << std::endl;
            }
        return nullptr;
    }

    /* 
        初始化日志，可选择的参数包括:
            日志文件名称
            缓冲区大小
            大行数
            日志阻塞队列的最大长度 
    */
    bool init(const char *file_name, int close_log, int log_buf_size = 8192, int split_lines = 5000000, int max_queue_size = 0);

    /* 将日志内容写入文件 */
    void write_log(int level, const char *format, ...);

    /* 将缓冲区的数据刷新到文件中 */
    void flush(void);

private:
    Log();  /* 私有的构造函数，用于实现懒加载 */
    virtual ~Log();

    /* 从阻塞队列中取日志写入文件 */
    void *async_write_log()
    {
        string single_log;

        /* 从阻塞队列中取出一个日志string，写入文件 */
        while (m_log_queue->pop(single_log))
        {
            locker_guard lock(m_mutex); /* 使用 lock_guard 自动管理互斥锁的生命周期 */
            fputs(single_log.c_str(), m_fp);
        }
    }

private:
    char dir_name[128];                 /* 路径名 */
    char log_name[128];                 /* log文件名 */
    int m_split_lines;                  /* 日志最大行数 */
    int m_log_buf_size;                 /* 日志缓冲区大小 */
    long long m_count;                  /* 日志行数记录 */
    int m_today;                        /* 因为按天分类,记录当前时间是哪一天 */
    FILE *m_fp;                         /* 打开log的文件指针 */
    char *m_buf;
    block_queue<string> *m_log_queue;   /* 阻塞队列 */
    bool m_is_async;                    /* 是否同步标志位 */
    locker m_mutex;
    int m_close_log;                    /* 关闭日志 */
};

/* 使用宏 LOG_DEBUG、LOG_INFO、LOG_WARN、LOG_ERROR 定义了日志记录的接口，简化调用 */
#define LOG_DEBUG(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log(0, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_INFO(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log(1, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_WARN(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log(2, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_ERROR(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log(3, format, ##__VA_ARGS__); Log::get_instance()->flush();}

#endif
