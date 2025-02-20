#ifndef _THREAD_H
#define _THREAD_H

/** 
 *    线程池
*/
#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>
#include "../lock/locker.h"
#include "../mysql/sql_connection_pool.h"

template<typename T>
class threadpool
{
public:
    threadpool(int m_actor_model, sqlconnection_pool *connpool, int thread_number = 8, int max_requests = 10000);
    ~threadpool();
    bool append(T* request, int state);
    bool append_p(T* request);

private:
    static void *work(void *arg);   // 工作线程任务
    void run();

private:
    int m_thread_number;            // 线程池中线程数量
    int m_max_requests;             // 请求队列最大请求数量
    pthread_t *m_threads;           // 线程数据
    std::list<T *>m_workqueue;      // 请求队列
    locker queuelock;               // 请求队列锁
    sem m_queuestat;                // 任务信号量
    sqlconnection_pool *m_connpool  // 数据库连接池
    int m_actor_model;              // 同步/异步模式
};


#endif
