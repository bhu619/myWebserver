#include "config.h"

/*构造函数*/
Config::Config(){
    
    PORT = 9006;        // 端口号,默认9006

    LOGWrite = 0;       // 日志写入方式，默认同步

    TRIGMode = 0;       // 触发组合模式,默认listenfd LT + connfd LT

    LISTENTrigmode = 0; // listenfd触发模式，默认LT

    CONNTrigmode = 0;   // connfd触发模式，默认LT

    OPT_LINGER = 0;     // 优雅关闭链接，默认不使用

    sql_num = 8;        // 数据库连接池数量,默认8

    thread_num = 8;     // 线程池内的线程数量,默认8

    close_log = 0;      // 是否关闭日志,默认开启日志

    actor_model = 0;    // 并发模型,默认是proactor
}

// /* 命令行参数解析 */
// void Config::parse_arg(int argc, char*argv[]){
//     int opt;
//     // 设置 optstring：选项字符
//     const char *str = "p:l:m:o:s:t:c:a:";
//     while ((opt = getopt(argc, argv, str)) != -1)
//     {
//         switch (opt)
//         {
//         case 'p':
//         {
//             PORT = atoi(optarg);
//             break;
//         }
//         case 'l':
//         {
//             LOGWrite = atoi(optarg);
//             break;
//         }
//         case 'm':
//         {
//             TRIGMode = atoi(optarg);
//             break;
//         }
//         case 'o':
//         {
//             OPT_LINGER = atoi(optarg);
//             break;
//         }
//         case 's':
//         {
//             sql_num = atoi(optarg);
//             break;
//         }
//         case 't':
//         {
//             thread_num = atoi(optarg);
//             break;
//         }
//         case 'c':
//         {
//             close_log = atoi(optarg);
//             break;
//         }
//         case 'a':
//         {
//             actor_model = atoi(optarg);
//             break;
//         }
//         default:
//             break;
//         }
//     }
// }

/* 显示帮助信息 */
void Config::display_usage() {
    printf(
        "用法: server [选项]...\n"
        "选项列表:\n"
        "  -p <端口号>           设置服务器监听端口号 (默认: 9006)\n"
        "  -l <日志写入方式>     设置日志写入方式 (0: 同步, 1: 异步, 默认: 0)\n"
        "  -m <触发模式>         设置触发模式 (0~3, 默认: 0)\n"
        "                         0: listenfd LT + connfd LT\n"
        "                         1: listenfd LT + connfd ET\n"
        "                         2: listenfd ET + connfd LT\n"
        "                         3: listenfd ET + connfd ET\n"
        "  -o <优雅关闭连接>     是否使用优雅关闭连接 (0: 不使用, 1: 使用, 默认: 0)\n"
        "  -s <数据库连接数>     设置数据库连接池连接数 (默认: 8)\n"
        "  -t <线程数>           设置线程池内线程数量 (默认: 8)\n"
        "  -c <关闭日志>         是否关闭日志 (0: 不关闭, 1: 关闭, 默认: 0)\n"
        "  -a <并发模型>         选择并发模型 (0: Proactor, 1: Reactor, 默认: 0)\n"
        "  -h                    显示此帮助信息\n"
        "示例:\n"
        "  server -p 8080 -t 16 -c 1\n"
    );
}

/* 解析命令行参数 */
void Config::parse_arg(int argc, char* argv[]) {
    int opt;

    // 设置 optstring：选项字符
    const char *str = ":p:l:m:o:s:t:c:a:h";
    while ((opt = getopt(argc, argv, str)) != -1) {
        switch (opt) {
            
            case 'p': // 设置端口号，范围应在 1 到 65535 之间。
                {
                    char *endptr;
                    PORT = strtol(optarg, &endptr, 10);
                    if (*endptr != '\0' || PORT <= 0 || PORT > 65535) {
                        fprintf(stderr, "无效的端口号：%s\n", optarg);
                        exit(EXIT_FAILURE);
                    }
                }
                break;
            
            case 'l':
                {
                    char *endptr;
                    LOGWrite = strtol(optarg, &endptr, 10);
                    if (*endptr != '\0' ) {
                        fprintf(stderr, "无效的日志写入方式：%s\n", optarg);
                        exit(EXIT_FAILURE);
                    }
                }
                break;
            
            case 'm':
                {
                    char *endptr;
                    TRIGMode = strtol(optarg, &endptr, 10);
                    if (*endptr != '\0' ) {
                        fprintf(stderr, "无效的触发模式：%s\n", optarg);
                        exit(EXIT_FAILURE);
                    }
                }
                break;

            case 'o':
                {
                    char *endptr;
                    OPT_LINGER = strtol(optarg, &endptr, 10);
                    if (*endptr != '\0' ) {
                        fprintf(stderr, "无效的优雅关闭选项：%s\n", optarg);
                        exit(EXIT_FAILURE);
                    }
                }
                break;

            case 's':
                {
                    char *endptr;
                    sql_num = strtol(optarg, &endptr, 10);
                    if (*endptr != '\0' || sql_num <= 0) {
                        fprintf(stderr, "无效的数据库连接池数量：%s，应为正整数\n", optarg);
                        exit(EXIT_FAILURE);
                    }
                }
                break;

            case 't':
                {
                    char *endptr;
                    thread_num = strtol(optarg, &endptr, 10);
                    if (*endptr != '\0' || thread_num <= 0) {
                        fprintf(stderr, "无效的线程数量：%s，应为正整数\n", optarg);
                        exit(EXIT_FAILURE);
                    }
                }
                break;

            case 'c':
                {
                    char *endptr;
                    close_log = strtol(optarg, &endptr, 10);
                    if (*endptr != '\0') {
                        fprintf(stderr, "无效的日志关闭选项：%s\n", optarg);
                        exit(EXIT_FAILURE);
                    }
                }
                break;

            case 'a':
                {
                    char *endptr;
                    actor_model = strtol(optarg, &endptr, 10);
                    if (*endptr != '\0' ) {
                        fprintf(stderr, "无效的并发模型选项：%s\n", optarg);
                        exit(EXIT_FAILURE);
                    }
                }
                break;

            case 'h': // 显示帮助信息
                display_usage();
                exit(EXIT_SUCCESS);

            case ':': // 缺少参数
                fprintf(stderr, "选项 -%c 缺少一个参数。\n", optopt);
                exit(EXIT_FAILURE);

            case '?':
                fprintf(stderr, "输入了未定义的选项：-%c\n", optopt);
                exit(EXIT_FAILURE);

            default:
                fprintf(stderr, "解析选项时遇到未知错误\n");
                exit(EXIT_FAILURE);
        }
    }

    // 检查是否有非选项参数
    if (optind < argc) {
        fprintf(stderr, "存在多余的非选项参数：\n");
        for (int i = optind; i < argc; i++) {
            fprintf(stderr, "  %s\n", argv[i]);
        }
        exit(EXIT_FAILURE);
    }
}