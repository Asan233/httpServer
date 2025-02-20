#include "time_wheel.h"
#include "../http/http_conn.h"

time_wheel::time_wheel() : cur_slot(0)
{
    for(int i = 0; i < N; ++i)
    {
        slot_head[i] = new tw_timer(0, 0);
        slot_tail[i] = new tw_timer(0, 0);
        slot_head[i]->next = slot_tail[i];
        slot_tail[i]->prev = slot_head[i];
    }
}

time_wheel::~time_wheel()
{
    for(int i = 0; i < N; ++i)
    {
        tw_timer* tmp = slot_head[i]->next;
        while(tmp != slot_tail[i])
        {
            slot_head[i]->next = tmp->next;
            delete tmp;
            tmp = slot_head[i]->next;
        }
        delete slot_head[i];
        delete slot_tail[i];
        slot_head[i] = slot_tail[i] = NULL;
    }
}

/* 将定时器插入合适的插槽中 */
void time_wheel::add_timer(tw_timer* timer)
{
    int slot = timer->time_slot;
    // 采用头插法将定时器插入对应的slot中
    slot_head[slot]->next->prev = timer;
    timer->next = slot_head[slot]->next;

    slot_head[slot]->next = timer;
    timer->prev = slot_head[slot];
}

/* 将定时器从slot链表中删除 */
void time_wheel::del_timer(tw_timer* timer)
{   
    if(!timer) return;
    // 从链表中取出timer
    timer->prev->next = timer->next;
    timer->prev->next->prev = timer->prev;
    delete timer;
}

/* 心跳函数 */
void time_wheel::tick()
{
    tw_timer* tmp = slot_head[cur_slot]->next;
    // 遍历触发当前slot指针上的定时器
    while(tmp != slot_tail[cur_slot])
    {
        tw_timer* next = tmp->next;
        if(tmp->rotation > 0)
        {
            --tmp->rotation;
        }else {
            tmp->cb_func(tmp->data_user);
            delete tmp;
        }
        tmp = next;
    }
    // 移动当前slot指针
    cur_slot = (cur_slot + 1) % N;
}

void Utils::init(int timeslot)
{
    m_TIMESLOT = timeslot;
}

// 设置文件描述符非阻塞
int Utils::setnonblocking(int fd)
{
    int old_opt = fcntl(fd, F_GETFL);
    int new_opt = old_opt | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_opt);
    return old_opt;
}

// 添加内核注册表读事件
void Utils::addfd(int epollfd, int fd, bool one_shot, int TRIGMode)
{
    epoll_event event;
    event.data.fd = fd;
    
    if(1 == TRIGMode)
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    else
        event.events = EPOLLIN | EPOLLRDHUP;
    
    if(one_shot)
        event.events |= EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    // 对于ET模式，必须设置非阻塞
    setnonblocking(fd);
}

// 信号处理函数
void Utils::sig_handler(int sig)
{
    // 为保证函数可重入性，保留原来的errno
    int save_error = errno;
    int msg = sig;
    send(u_pipefd[1], (char *)&msg, 1, 0);
    errno = save_error;
}

// 设置信号函数
void addsig(int sig, void(handler)(int), bool restart = true)
{
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    if(restart)
        sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}

// 定时处理任务，触发tick心跳，并不断发送SIGALRM信号
void Utils::timer_handler()
{
    m_time_wheel.tick();
    alarm(m_TIMESLOT);
}

/* 连接超时回调函数 */
void cb_func(client_data* user_data)
{
    epoll_ctl(Utils::u_epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
    assert(user_data);
    close(user_data->sockfd);
    http_conn::m_user_count--;
}

