#ifndef _LOG_H
#define _LOG_H

/**
 *   同步/异步日志系统
 *   1. 日志模块： 实现同步/异步的日志写入
 *   2. 阻塞队列模块： 主要解决异步日志写入的准备
*/

#include <stdio.h>
#include <iostream>
#include <string>
#include <stdarg.h>
#include <pthread.h>
#include "block_queue.h"

/*
*   使用单例模式，确保整个程序只有一个Log实例
*/

class Log
{
public:
    // C++11特性：对静态局部变量的多线程访问不用加锁，编译器会确保静态局部变量实例化只有一个线程能够进行，其他线程无法进行, 保证了静态局部变量初始化的原子性
    static Log* get_instance()
    {
        static Log instance;
        return &instance;
    }

    static void *flush_log_thread(void *args)
    {
        Log::get_instance()->async_write_log();
    }

    // 初始化日志类：日志文件、日志缓冲区大小、最大行数以及最长日志条队列
    bool init(const char *file_name, int close_log, int log_buf_size = 8192, int split_line = 5000000, int max_queue_size = 0);
    // 写日志
    void write_log(int level, const char *format, ...);

    void flush(void);

private:
    Log();
    virtual ~Log();
    void *async_write_log()
    {
        std::string single_log;
        // 从阻塞队列中取出一条日志
        while(m_log_queue->pop(single_log))
        {
            m_mutex.lock();
            fputs(single_log.c_str(), m_fp);
            m_mutex.unlock();
        }
    }

private:
    char dir_name[128];     // 路径名
    char log_name[128];     // log文件名
    int m_split_lines;      // 日志最大行数
    int m_log_buf_size;     // 日志缓冲区大小
    long long m_count;      // 日志行数记录
    int m_today;            // 因为日志按天分类，因此记录每天的号数
    FILE *m_fp;             // 日志文件指针
    char *m_buf;
    block_queue<std::string> *m_log_queue;  // 阻塞队列
    bool m_is_async;        // 是否同步
    locker m_mutex;
    int m_close_log;        // 关闭日志
};

#define LOG_DEBUG(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log(0, format, ##__VA_ARGS__); Log::get_instance()->flush(); }
#define LOG_INFO(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log(1, format, ##__VA_ARGS__); Log::get_instance()->flush(); }
#define LOG_WARN(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log(2, format, ##__VA_ARGS__); Log::get_instance()->flush(); }
#define LOG_ERROR(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log(3, format, ##__VA_ARGS__); Log::get_instance()->flush(); }

#endif
