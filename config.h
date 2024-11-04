#ifndef CONFIG_H
#define CONFIG_H

#include "webserver.h"

using namespace std;

class Config
{
public:
    Config();
    ~Config(){};

    void parse_arg(int argc, char*argv[]);      /* 使用 getopt 函数来解析命令行参数 */

    void display_usage();                       /* 显示帮助信息 */

    int PORT;           // 端口号 (默认为9006)

    int LOGWrite;       // 日志写入方式 (默认同步写入且不关闭)

    int TRIGMode;       // 触发组合模式

    int LISTENTrigmode; // listenfd触发模式

    int CONNTrigmode;   // connfd触发模式

    int OPT_LINGER;     // 优雅关闭链接

    int sql_num;        // 数据库连接池数量

    int thread_num;     // 线程池内的线程数量

    int close_log;      // 是否关闭日志

    int actor_model;    // 并发模型选择 (默认使用 Proactor 模式)
};

#endif
