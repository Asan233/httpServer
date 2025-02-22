#ifndef _THREADPOOL_H
#define _THREADPOOL_H

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
    sqlconnection_pool *m_connpool; // 数据库连接池
    int m_actor_model;              // 同步/异步模式
};


template<typename T>
/* 事件驱动模式， 数据库连接池， 线程数量， 请求队列大小 */
threadpool<T>::threadpool(int m_actor_model, sqlconnection_pool* connpool, int thread_number, int max_requests)
{
    if(thread_number <= 0 || max_requests <= 0)
        throw std::exception();
    
    m_threads = new pthread_t[thread_number];
    if(!m_threads)
        throw std::exception();

    for(int i = 0; i < thread_number; ++i)
    {
        if(pthread_create(m_threads + i, NULL, work, this) != 0)
        {
            delete[] m_threads;
            throw std::exception();
        }
        if(pthread_detach(m_threads[i]))
        {
            delete[] m_threads;
            throw std::exception();
        }
    }
}

template<typename T>
threadpool<T>::~threadpool()
{
    delete[] m_threads;
}

template<typename T>
bool threadpool<T>::append(T* request, int state)
{
    queuelock.lock();
    if(m_workqueue.size() >= m_max_requests)
    {
        queuelock.unlock();
        return false;
    }
    request->m_state = state;
    m_workqueue.push_back(request);
    queuelock.unlock();
    // 通知休眠的工作进程
    m_queuestat.post();
    return true;
}

template<typename T>
bool threadpool<T>::append_p(T* request)
{
    queuelock.lock();
    if(m_workqueue.size() >= m_max_requests)
    {
        queuelock.unlock();
        return false;
    }
    m_workqueue.push_back(request);
    queuelock.unlock();
    // 通知休眠的工作进程
    m_queuestat.post();
    return true;
}

template<typename T>
void *threadpool<T>::work(void *arg)
{
    threadpool *pool = (threadpool *)arg;
    pool->run();
    return pool;
}

template<typename T>
void threadpool<T>::run()
{
    while(true)
    {
        m_queuestat.wait();
        queuelock.lock();
        if(m_workqueue.empty())
        {
            queuelock.unlock();
            continue;
        }
        T* request = m_workqueue.front();
        m_workqueue.pop_front();
        queuelock.unlock();
        if(!request)
            continue;
        
        if(1 == m_actor_model)
        {
            // 1 表示工作线程启动reactor模式，工作线程进行 读、写和逻辑处理
            if(0 == request->m_state)
            {
                // 连接有数据需要处理读
                if(request->read_once())
                {
                    request->improv = 1;
                    sqlconnectionRAII mysqlcon(&request->mysql, m_connpool);
                    request->process();
                }else {
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            }else {
                // 连接有数据需要写
                if(request->write())
                {
                    request->improv = 1;
                }else {
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            }
        }else {
            // 0 表示工作线程启动proactor模式，工作线程只进行逻辑处理
            sqlconnectionRAII mysqlcon(&request->mysql, m_connpool);
            request->process();
        }
    }
}








#endif
