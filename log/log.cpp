#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <stdarg.h>
#include "log.h"
#include <pthread.h>

using namespace std;

Log::Log() {
    m_count = 0;            /* 初始化日志行数计数器 */
    m_is_async = false;     /* 初始化为同步日志模式 */
}

Log::~Log()
{
    if (m_fp != nullptr)
    {
        fflush(m_fp);
        fclose(m_fp);
    }

    if (m_is_async)
    {
        delete m_log_queue;
    }

    delete[] m_buf;
}

bool Log::init(const char *file_name, int close_log, int log_buf_size, int split_lines, int max_queue_size)
{
    /* 
        如果设置了max_queue_size,则设置为异步日志模式
        异步模式下，使用阻塞队列存储日志信息，并通过后台线程异步写入日志
    */
    if (max_queue_size >= 1)
    {
        m_is_async = true;
        m_log_queue = new block_queue<string>(max_queue_size);
        pthread_t tid;

        if (pthread_create(&tid, NULL, flush_log_thread, NULL) != 0)
        {
            std::cerr << "Failed to create log thread." << std::endl;
            delete m_log_queue;
            return false;
        }
        pthread_detach(tid);  // 设置线程为分离状态，自动释放资源
    }
    
    m_close_log = close_log;
    m_log_buf_size = log_buf_size;
    m_buf = new char[m_log_buf_size];       /* 用于日志的缓存，在初始化时分配缓冲区的大小 */
    memset(m_buf, '\0', m_log_buf_size);
    m_split_lines = split_lines;            /* 当日志文件的行数达到 m_split_lines 的值时，程序会自动创建一个新的日志文件以继续记录，避免单个日志文件过大 */

    time_t t = time(NULL);                  /* 获取当前的系统时间，返回的是从 1970 年 1 月 1 日（即 Unix 纪元）到现在的秒数 */
    struct tm my_tm;                        /* 包含了当前的本地时间信息 */
    if (localtime_r(&t, &my_tm) == nullptr) {
        std::cerr << "Failed to get local time." << std::endl;
        delete[] m_buf;
        if (m_is_async) delete m_log_queue;
        return false;                       /* 初始化失败 */
    }
        
    const char *p = strrchr(file_name, '/');    /* 指向字符串中最后一个 '/' 的位置，用于提取文件名部分 */
    char log_full_name[256] = {0};              /* 存储日志文件的完整路径，最大长度为 256 字节 */

    if (p == NULL)  /* file_name中没有路径 */
    {
        snprintf(log_full_name, 255, "%d_%02d_%02d_%s", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, file_name);
    }
    else
    {
        strcpy(log_name, p + 1);    /* log_name存储日志文件名称 */
        strncpy(dir_name, file_name, p - file_name + 1);
        snprintf(log_full_name, 255, "%s%d_%02d_%02d_%s", dir_name, my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, log_name);
    }

    m_today = my_tm.tm_mday;        /* 表示当前日期的天 */
    
    m_fp = fopen(log_full_name, "a");   /* 参数 "a" 表示以 追加模式 打开日志文件：如果文件已经存在，日志将追加到文件末尾；如果文件不存在，会创建一个新的文件。*/
    if (m_fp == NULL)
    {
        std::cerr << "Failed to open log file: " << log_full_name << std::endl;
        delete[] m_buf;
        if (m_is_async) delete m_log_queue;
        return false;               /* 初始化失败 */
    }

    return true;
}

void Log::write_log(int level, const char *format, ...)
{
    struct timeval now = {0, 0};/* 通常包含两个成员：一个是表示秒数的tv_sec成员，另一个是表示微秒数的tv_usec成员 */
    gettimeofday(&now, NULL);   /* 获取当前时间的秒数和微秒数 */
    time_t t = now.tv_sec;
    struct tm my_tm;
    localtime_r(&t, &my_tm);    /* 线程安全的时间获取 */

    char s[16] = {0};

    /* 设置日志等级 */
    switch (level)
    {
    case 0:
        strcpy(s, "[debug]:");
        break;
    case 1:
        strcpy(s, "[info]:");
        break;
    case 2:
        strcpy(s, "[warn]:");
        break;
    case 3:
        strcpy(s, "[erro]:");
        break;
    default:
        strcpy(s, "[info]:");
        break;
    }

    
    {
        locker_guard lock(m_mutex); /* 写入日志前加锁 */
        m_count++;                             /* 当前日志行数 */

        /* 日志文件切分逻辑 */
        if (m_today != my_tm.tm_mday || m_count % m_split_lines == 0) //everyday log
        {      
            char new_log[256] = {0};
            fflush(m_fp);
            fclose(m_fp);

            char tail[16] = {0};
            snprintf(tail, 16, "%d_%02d_%02d_", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday);
        
            if (m_today != my_tm.tm_mday)
            {
                snprintf(new_log, 255, "%s%s%s", dir_name, tail, log_name);
                m_today = my_tm.tm_mday;
                m_count = 0;
            }
            else
            {
                snprintf(new_log, 255, "%s%s%s.%lld", dir_name, tail, log_name, m_count / m_split_lines);
            }
            m_fp = fopen(new_log, "a");
            if (m_fp == nullptr)
            {
                std::cerr << "Failed to open log file: " << new_log << std::endl;
                return;
            }
        }
    }

    va_list valst;              /* va_list 是处理可变参数的标准工具 */
    va_start(valst, format);    /* 初始化 va_list 变量 */

    string log_str;
    {
        locker_guard lock(m_mutex);

        //写入的具体时间内容格式
        int n = snprintf(m_buf, 48, "%d-%02d-%02d %02d:%02d:%02d.%06ld %s ",
                        my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday,
                        my_tm.tm_hour, my_tm.tm_min, my_tm.tm_sec, now.tv_usec, s);
        
        int m = vsnprintf(m_buf + n, m_log_buf_size - n - 1, format, valst);
        m_buf[n + m] = '\n';
        m_buf[n + m + 1] = '\0';
        log_str = m_buf;
    }

    va_end(valst);              /* 结束对可变参数的访问 */

    /* 异步写入 && 阻塞队列未满 && 阻塞队列指针非空*/
    if (m_is_async && !m_log_queue->full() && m_log_queue != nullptr)
    {
        m_log_queue->push(log_str); /* 将日志字符串 log_str 推入日志队列 */
    }
    else
    {
        /* 如果队列已满，选择丢弃当前日志，并增加 dropped_logs 计数器。 */
        static int dropped_logs = 0;
        if (m_is_async && m_log_queue->full())
        {
            dropped_logs++;
            if (dropped_logs % 100 == 0)
            {
                /* 每丢弃 100 条日志时，通过标准错误流（std::cerr）输出警告，提醒队列已满 */
                std::cerr << "Log queue full. Dropped logs count: " << dropped_logs << std::endl;
            }
        }
        else
        {
            /* 同步写入日志文件 */
            locker_guard lock(m_mutex);
            fputs(log_str.c_str(), m_fp);
        }
    }
}

void Log::flush(void)
{
    locker_guard lock(m_mutex); // 使用 lock_guard 自动管理锁的生命周期
    if (m_fp != nullptr)
    {
        fflush(m_fp); //  强制刷新输出缓冲区，将缓冲区中的内容立即写入到文件中，确保日志内容不会滞留在缓冲区
    }
}
