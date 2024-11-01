#include <mysql/mysql.h>
#include <stdio.h>
#include <string>
#include <string.h>
#include <stdlib.h>
#include <list>
#include <pthread.h>
#include <iostream>
#include "sql_connection_pool.h"

using namespace std;

/* 构造函数 */
connection_pool::connection_pool()
{
	m_CurConn = 0;	/* 当前已使用的连接数 */
	m_FreeConn = 0;	/* 当前空闲的连接数 */
}

connection_pool *connection_pool::GetInstance()
{
	static connection_pool connPool;	/* C++11 之后，可以通过 static 局部变量来实现线程安全的单例模式 */
	return &connPool;
}

/* 构造初始化 */
void connection_pool::init(string url, string User, string PassWord, string DBName, int Port, int MaxConn, int close_log)
{
	m_url = url;
	m_Port = Port;
	m_User = User;
	m_PassWord = PassWord;
	m_DatabaseName = DBName;
	m_close_log = close_log;

	for (int i = 0; i < MaxConn; i++)
	{
		MYSQL *raw_con = mysql_init(NULL);		/* 初始化 MYSQL 连接句柄 */

		if (raw_con == NULL)
		{
			LOG_ERROR("MySQL init Error: %s", mysql_error(raw_con));
			continue;  							/* 记录错误但不退出程序 */ 
		}

		/* 连接到数据库 */
		raw_con = mysql_real_connect(raw_con, url.c_str(), User.c_str(), PassWord.c_str(), DBName.c_str(), Port, NULL, 0);

		if (raw_con == NULL)
		{
			LOG_ERROR("MySQL init Error: %s", mysql_error(raw_con));
			mysql_close(raw_con);  				/* 释放当前连接资源 */
			continue;  							/* 记录错误但不退出程序 */
		}

		/* 使用 shared_ptr 来管理 MYSQL 连接 */
        std::shared_ptr<MYSQL> con(raw_con, [](MYSQL* ptr) {
            mysql_close(ptr); 					/* 自定义删除器，用于释放 MYSQL 连接 */
        });

		connList.push_back(con);				/* 将连接加入连接池 */
		++m_FreeConn;							/* 增加空闲连接计数 */
	}

	/* 如果没有任何连接被成功创建 */ 
    if (m_FreeConn == 0)
    {
        LOG_ERROR("All database connections failed to initialize.");
        exit(1);  								/* 没有连接被创建成功，退出程序 */
    }

	reserve = sem(m_FreeConn);					/* 初始化信号量，用于管理空闲连接的数量 */
	m_MaxConn = m_FreeConn;						/* 设置最大连接数 */
}


/* 获取连接时，直接从 connList 中弹出一个 std::shared_ptr<MYSQL> */
std::shared_ptr<MYSQL> connection_pool::GetConnection()
{	
	reserve.wait();   				/* 阻塞等待信号量，确保有可用连接 */

	lock.lock();

	if (connList.empty())
    {
        lock.unlock();
        return nullptr;
    }

	std::shared_ptr<MYSQL> con = connList.front();
	connList.pop_front();

	--m_FreeConn;
	++m_CurConn;

	lock.unlock();

	return con;
}

/* 释放当前使用的连接 */
bool connection_pool::ReleaseConnection(std::shared_ptr<MYSQL> con)
{
	if (con == nullptr)
    {
        return false;
    }

	lock.lock();

	connList.push_back(con);		/* 将连接放回连接池 */
	++m_FreeConn;
	--m_CurConn;

	lock.unlock();

	reserve.post();					/* 增加信号量，释放连接 */

	return true;
}

/* 销毁数据库连接池 */
void connection_pool::DestroyPool()
{

	lock.lock();
    if (!connList.empty())
    {
        connList.clear();  // 清空列表，智能指针会自动释放所有连接
    }
    m_CurConn = 0;
    m_FreeConn = 0;
    lock.unlock();
}

/* 返回当前空闲的连接数 */
int connection_pool::GetFreeConn()
{
	return this->m_FreeConn;
}

/* 析构函数 */
connection_pool::~connection_pool()
{
	DestroyPool();
}