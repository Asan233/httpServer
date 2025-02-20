#ifndef _SQL_CONNECTION_H
#define _SQL_CONNECTION_H

/**
 *       数据库连接池类
 *    使用单例模式，保证连接池的唯一
 *    使用互斥锁保证连接访问的线程安全
 *          连接池为静态大小
*/

#include <stdio.h>
#include <list>
#include <mysql/mysql.h>
#include <error.h>
#include <string.h>
#include <iostream>
#include <string>
#include "../lock/locker.h"
#include "../log/log.h"


class sqlconnection_pool
{
public:
    MYSQL* GetConnection();                     // 获取数据库连接
    bool ReleaseConnection(MYSQL* conn);        // 释放连接
    int GetFreeConn();                          // 得到空闲数据库连接数量
    void DestroyPool();                         // 销毁连接池

    // 单例模式
    static sqlconnection_pool* GetInstance();      // 单例获得
    // 数据库连接池初始化
    void init(std::string url, std::string User, std::string PassWord, std::string DataBaseName, int Port, int MaxConn, int close_log);

private:
    sqlconnection_pool();
    ~sqlconnection_pool();

    int m_MaxConn;          // 最大连接数
    int m_CurConn;          // 已经分配连接数
    int m_FreeConn;         // 空闲连接数
    locker lock;            // 互斥锁
    std::list<MYSQL*> connList;     // 连接链表
    sem reserve;            // 信号量

public:
    std::string m_url;          // 数据库地址
    std::string m_port;         // 端口
    std::string m_User;         // 数据库用户名
    std::string m_PassWord;     // 数据库用户密码
    std::string m_Databasename; // 数据库名
    int m_close_log;            // 日志开关
};


class sqlconnectionRAII{

public:
    sqlconnectionRAII(MYSQL **con, sqlconnection_pool *connPool);
    ~sqlconnectionRAII();

private:
    MYSQL *conRAII;
    sqlconnection_pool *poolRAII;
};

#endif
