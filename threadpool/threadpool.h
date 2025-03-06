#ifndef _THREADPOOL_H
#define _THREADPOOL_H

/** 
 *    线程池
*/
#include <list>
#include <cstdio>
#include <exception>
#include <vector>
#include <thread>
#include "../lock/locker.h"
#include "../mysql/sql_connection_pool.h"
#include "../log/block_queue.h"


template<typename T>
class threadpool
{
public:
    threadpool(int m_actor_model, sqlconnection_pool *connpool, int thread_number = 8, int max_requests = 10000);
    ~threadpool();
    bool append(T* request, int state);
    bool append_p(T* request);

private:
    static void *work(threadpool<T> *arg);   // 工作线程任务
    void run();

private:
    int m_thread_number;                            // 线程池中线程数量
    int m_max_requests;                             // 请求队列最大请求数量
    std::vector<std::thread> m_threads;             // 线程数据
    block_queue<T *>m_workqueue;                    // 请求队列

    sqlconnection_pool *m_connpool;                 // 数据库连接池
    int m_actor_model;                              // 同步/异步模式
    int m_close_log = 0;                            // 日志开启
};


template<typename T>
/* 事件驱动模式， 数据库连接池， 线程数量， 请求队列大小 */
threadpool<T>::threadpool(int actor_model, sqlconnection_pool* connpool, int thread_number, int max_requests)
{
    if(thread_number <= 0 || max_requests <= 0)
        throw std::exception();
    
    m_threads.reserve(thread_number);

    for(int i = 0; i < thread_number; ++i)
    {
        m_threads[i] = std::thread(work, this);
    }
    m_max_requests = max_requests;
    m_thread_number = thread_number;
    m_actor_model = actor_model;
    m_connpool = connpool;
    LOG_INFO("ThreadPool init successfull : thread_numver : %d, max_request : %d", m_thread_number, m_max_requests);
}

template<typename T>
threadpool<T>::~threadpool() {}

template<typename T>
bool threadpool<T>::append(T* request, int state)
{
    bool ret = request->m_state = state;
    m_workqueue.push(request);

    LOG_INFO("thread Push the client(%s)", inet_ntoa(request->get_address()->sin_addr));
    return ret;
}

template<typename T>
bool threadpool<T>::append_p(T* request)
{
    bool ret = m_workqueue.push(request);
    LOG_INFO("thread Push the client(%s)", inet_ntoa(request->get_address()->sin_addr));
    return ret;
}

template<typename T>
void *threadpool<T>::work(threadpool<T>* pool)
{
    pool->run();
    return pool;
}

template<typename T>
void threadpool<T>::run()
{
    while(true)
    {
        T* request;
        bool ret = m_workqueue.pop(request);
        //std::cout << ret  << std::endl;
        if(!ret)
            continue;
        
        LOG_INFO( "thread Get the client(%s)", inet_ntoa(request->get_address()->sin_addr) );
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
            //LOG_INFO("Start Proactor Process");
            request->process();
        }
    }
}


#endif
