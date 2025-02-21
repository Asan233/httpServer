#ifndef _WEBSERVER_H
#define _WEBSERVER_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <cassert>
#include <sys/epoll.h>
#include <string>

#include "mysql/sql_connection_pool.h"
#include "log/log.h"
#include "timer/time_wheel.h"
#include "threadpool/threadpool.h"
#include "http/http_conn.h"
#include "timer/time_wheel.h"

const int MAX_FD = 65536;               // 最大文件描述符
const int MAX_EVENT_NUMBER = 10000;     // 最大事件数
const int TIMESLOT = 5;                 // 最小Tick单位时间w

/**
 *      后台服务器类
 *      并发使用 半同步/半异步模型
*/
class WebServer
{
public:
    WebServer();
    ~WebServer();

    /**
     *          初始化服务器
     *    数据库 ： 数据库用户， 数据库用户密码， 数据库名字
     *    日志： 是否写日志， 是否保持连接， 事件触发模式， sql连接数量
     *    线程： 线程数量， 是否关闭日志， 服务器同步/异步 模式
    */
    void init(int port, std::string user, std::string passWord, std::string databaseName,
              int log_write, int opt_linger, int trigmode, int sql_num,
              int thread_num, int close_log, int actor_model);
    
    void thread_pool();     // 线程池初始化
    void sql_pool();        // 数据库连接池初始化
    void log_write();       // 日志初始化
    void trig_mode();       // 服务器触发模式设置
    void eventListen();     // 监听服务器事件
    void eventLoop();       // 服务器启动
    void timer(int connfd, struct sockaddr_in client_address);           // 服务器定时器
    void adjust_timer(tw_timer *timer);
    void deal_timer(tw_timer *timer, int sockfd);
    bool dealclientdata();
    bool dealwithsignal(bool &timeout, bool &stop_server);
    void dealwithread(int sockfd);
    void dealwithwrite(int sockfd);

public:
    /* 服务器基本参数 */
    int m_port;         // 端口号
    char *m_root;       // 跟文件路径
    int m_log_write;    // 是否异步写日志
    int m_close_log;    // 是否启动日志
    int m_actormodel;   // 服务器 同步/异步 模式

    int m_pipefd[2];    // 信号管道
    int m_epollfd;      // 事件监听符
    http_conn *users;   // HTTP类

    /* 数据库相关 */
    sqlconnection_pool *m_sqlconnectionPool;        // 数据库连接池
    std::string m_user;                             // 数据库用户
    std::string m_passWord;                         // 数据库用户密码
    std::string m_databaseName;                     // 数据库名称
    int m_sql_num;                                  // 使用sql连接数量

    /* 线程池相关 */
    threadpool<http_conn> *m_pool;                  // 线程池
    int m_thread_num;                               // 线程数量

    /* epoll_event */
    epoll_event events[MAX_EVENT_NUMBER];           // event事件数量

    int m_listenfd;                                 // socket监听
    int m_OPT_LINGER;                               // 是否Linger
    int m_TRIGMode;                                 // 事件触发模式
    int m_LISTENTrigmode;                           // Listen触发模式
    int m_CONNTrigMode;                             // 连接触发模式

    /* 定时器 */
    client_data *users_timer;
    Utils utils;
};



#endif
