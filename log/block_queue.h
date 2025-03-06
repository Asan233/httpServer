#ifndef _BLOCK_QUEUE_H
#define _BLOCK_QUEUE_H

#include <iostream>
#include <stdlib.h>
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include "../lock/locker.h"

/**
 *    阻塞队列
 *    双缓冲队列： 一个生产者队列，一个消费者队列，消费者队列为空时，交换生产者与消费者队列
*/

template< typename T>
class block_queue
{
public:
    block_queue(int max_size = 1000) : m_size(0), nonblock(false)
    {
        if(max_size <= 0)
        {
            exit(-1);
        }
        m_max_size = 1000;
    }

    ~block_queue() {}

    void clear()
    {
        std::lock_guard<std::mutex> lock(producerMutex);
        nonblock = true;
        not_empty.notify_all();
    }
    
    // 判断是否队列已满
    bool full()
    {
        int tmp = m_size.load(std::memory_order_seq_cst);
        return tmp == m_max_size;
    }

    // 判断队列是否为空
    bool empty()
    {
        return m_size.load(std::memory_order_seq_cst) == 0;
    }

    int size()
    {
        int tmp = m_size.load(std::memory_order_seq_cst);
        return tmp;
    }

    int max_size()
    {
        return m_max_size;
    }

    /**
     *  往队尾添加元素，需要将所有消费队列元素的线程唤醒
     *  队列放入元素相当于生产者生产一个元素
    */
   bool push(const T& item)
   {
        std::unique_lock<std::mutex> lock(producerMutex);
        producerQueue.push(item);
        m_size.fetch_add(1, std::memory_order_seq_cst);
        not_empty.notify_one();
        return true;
   }

    bool pop(T &item)
    {
        std::unique_lock<std::mutex> lock(consumerMutex);
        if(consumerQueue.empty() && swapQueue() == 0) {
            return false;
        }
        item = consumerQueue.front();
        consumerQueue.pop();
        m_size.fetch_add(-1, std::memory_order_seq_cst);
    }

    // 增加了超时处理
    bool pop(T &item, std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(consumerMutex);
        if(consumerQueue.empty() && swapQueue(timeout) == 0) {
            return false;
        }
        item = consumerQueue.front();
        consumerQueue.pop();
        m_size.fetch_add(-1, std::memory_order_seq_cst);
        return true;
    }

    // 交换消费者队列与生产者队列
    int swapQueue() {
        std::unique_lock<std::mutex> lock(producerMutex);
        not_empty.wait(lock, [this] { return !producerQueue.empty() || nonblock; });
        std::swap(producerQueue, consumerQueue);
        return consumerQueue.size();
    }

    // 增加了超时返回
    int swapQueue(std::chrono::milliseconds timeout){
        std::unique_lock<std::mutex> lock(producerQueue);
        not_empty.wait_for(lock, timeout, [this] { return !producerQueue.empty() || nonblock; });
        std::swap(producerQueue, consumerQueue);
        return consumerQueue.size();
    }

private:
    // 互斥锁，实现线程安全访问队列
    std::mutex producerMutex;       // 生产者互斥锁
    std::mutex consumerMutex;       // 消费者互斥锁
    std::condition_variable not_empty;  // 条件变量

    bool nonblock;      // 是否启用工作队列

    std::queue<T> producerQueue;    // 生产者队列
    std::queue<T> consumerQueue;    // 消费者队列

    // 队列最大容量
    int m_max_size;
    // 队列中的元素, 使用原子序列
    std::atomic<int> m_size;
    
};

#endif
