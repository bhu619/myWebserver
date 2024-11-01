#ifndef _CONNECTION_POOL_
#define _CONNECTION_POOL_

#include <stdio.h>
#include <list>
#include <mysql/mysql.h>
#include <error.h>
#include <string.h>
#include <iostream>
#include <string>
#include <memory> // 包含智能指针的头文件
#include "../lock/locker.h"
#include "../log/log.h"

using namespace std;

class connection_pool
{
public:
	std::shared_ptr<MYSQL> GetConnection(); /* 获取一个可用的数据库连接 */
	bool   ReleaseConnection(std::shared_ptr<MYSQL> con);	/* 释放一个数据库连接，将连接归还到连接池中，使其可以被其他请求再次使用 */
	int    GetFreeConn();					/* 获取当前空闲连接数 */
	void   DestroyPool();					/* 销毁连接池 */

	static connection_pool *GetInstance();	/* 单例模式 */

	/* 
		初始化连接池，设置数据库的连接信息（包括主机地址、端口号、用户名、密码等），并建立指定数量的数据库连接
		MaxConn 指示了数据库连接池的大小
	*/
	void init(string url, string User, string PassWord, string DataBaseName, int Port, 
					int MaxConn, int close_log); 

private:
	connection_pool();
	~connection_pool();

	int m_MaxConn;  						/* 最大连接数 */
	int m_CurConn;  						/* 当前已使用的连接数 */
	int m_FreeConn; 						/* 当前空闲的连接数 */
	locker lock;
	list<shared_ptr<MYSQL>> connList; 		/* 数据库连接池，该列表管理了所有的数据库连接 */
	sem reserve;							/* 信号量 */

public:
	string m_url;			 				/* 主机地址 */
	string m_Port;							/* 数据库端口号 */
	string m_User;							/* 登陆数据库用户名 */
	string m_PassWord;	 					/* 登陆数据库密码 */
	string m_DatabaseName;					/* 使用数据库名 */
	int m_close_log;						/* 日志开关 */
};


/* 实现 RAII（资源获取即初始化）模式，确保数据库连接在使用结束后能自动释放 */
class connectionRAII
{
public:
    // 构造函数，从连接池中获取连接并持有
    connectionRAII(std::shared_ptr<MYSQL> &con, connection_pool *connPool)
    {
        // 获取连接池中的一个连接
        con = connPool->GetConnection();
        if (con == nullptr)
        {
            throw std::runtime_error("Failed to get a valid database connection.");
        }
        
        // 将连接和连接池保存到成员变量中
        conRAII = con;
        poolRAII = connPool;
    }

    // 析构函数
    ~connectionRAII()
    {
        // 使用 shared_ptr 的自动管理特性进行释放
        if (conRAII)
        {
            poolRAII->ReleaseConnection(conRAII);
        }
    }

private:
    std::shared_ptr<MYSQL> conRAII;
    connection_pool *poolRAII;
};


#endif
