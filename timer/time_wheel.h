#ifndef _TIME_WHEEL_H
#define _TIME_WHEEL_H

#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <netinet/in.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/epoll.h>
#include <assert.h>
#include "../log/log.h"


class tw_timer;             // 时间论定时器声明
/* 用户连接数据，用于关联一个用户与一个定时器 */
struct client_data
{
    sockaddr_in address;
    int sockfd;
    tw_timer* timer;
};

/* 定时器，每一个用户连接绑定一个定时，该定时器用于时间论进行交互 */
struct tw_timer
{
public:
    tw_timer(int rot, int ts) : rotation(rot), time_slot(ts), next(NULL), prev(NULL) {}

public:
    int rotation;                         // 表示经过几次时间轮周期后触发超时
    int time_slot;                        // 时间轮插槽
    void (*cb_func)(client_data *);       // 定时器超时回调函数
    struct client_data* data_user;
    tw_timer* prev;                       // 前一个定时器
    tw_timer* next;                       // 后一个定时器
};

/* 时间轮类*/
class time_wheel
{
public:
    time_wheel();
    ~time_wheel();

    void add_timer(tw_timer* timer);
    void del_timer(tw_timer* timer);
    void adjust_timer(tw_timer* timer);
    void tick();

private:
    static const int N = 128;               // 时间轮的插槽数量
    int cur_slot;                           // 当前指针指向什么插槽
    tw_timer* slot_head[N];                 // 每个插槽的头指针，方便插入与删除定时器
    tw_timer* slot_tail[N];                 // 每个插槽的尾指针，方便插入与删除定时器
    int m_close_log = 0;
};

void cb_func(client_data* user_data);       //定时器回调函数

class Utils
{
public:
    Utils() {}
    ~Utils() {}

    void init(int timeslot);

    // 设置文件描述符非阻塞
    int setnonblocking(int fd);

    // 内核事件表注册 读事件、ET模式，选择开启EPOLLONSHOT
    void addfd(int epollfd, int fd, bool one_shot, int TRIGMode);

    // 信号处理函数
    static void sig_handler(int sig);

    // 添加信号函数
    void addsig(int sig, void(handler)(int), bool restart = true);

    // 定时器处理任务，重新定时以不断发送SIGALRM信号
    void timer_handler();

    void show_error(int connfd, const char*info);

public:
    static int* u_pipefd;
    time_wheel m_time_wheel;
    static int u_epollfd;
    int m_TIMESLOT;
};

#endif
