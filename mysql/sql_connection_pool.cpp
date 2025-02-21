#include "sql_connection_pool.h"

sqlconnection_pool::sqlconnection_pool()
{
    m_CurConn = 0;
    m_FreeConn = 0;
}

/* 单例模式 */
sqlconnection_pool* sqlconnection_pool::GetInstance()
{
    static sqlconnection_pool connPool;
    return &connPool;
}


// 构造函数初始化
void sqlconnection_pool::init(std::string url, std::string User, std::string PassWord, std::string DBName, int Port, int MaxConn, int close_log)
{
    /* 初始化连接池成员变量 */
    m_url = url;
    m_port = Port;
    m_User = User;
    m_PassWord = PassWord;
    m_Databasename = DBName;
    m_close_log = close_log;

    for(int i = 0; i < MaxConn; ++i)
    {
        MYSQL *con = NULL;
        // 创建mysql连接
        con = mysql_init(con);
        if(NULL == con)
        {
            LOG_ERROR("MYSQL error");
            exit(1);
        }
        
        // 真正连接MySQL服务器
        con = mysql_real_connect(con, m_url.c_str(), 
                                 m_User.c_str(), m_PassWord.c_str(), m_Databasename.c_str(), 
                                 Port, NULL, 0);
        if(NULL == con)
        {
            LOG_ERROR("MYSQL Error");
            exit(1);
        }
        connList.push_back(con);
        ++m_FreeConn;
    }
    // 初始化连接池信号量，数量初始化跟创建的连接池一样
    reserve = sem(m_FreeConn);

    m_MaxConn = m_FreeConn;
}


/*  从连接池返回一个空闲的连接 */
MYSQL* sqlconnection_pool::GetConnection()
{
    MYSQL* conn = NULL;
    
    if(0 == connList.size())
        return NULL;
    
    // 获取信号量，如果有连接则信号量减1
    reserve.wait();
    
    // 互斥访问 连接池链表
    lock.lock();

    // 从连接池中取出连接
    conn = connList.front();
    connList.pop_front();

    // 修改连接池数据结构
    --m_FreeConn;
    ++m_CurConn;

    lock.unlock();

    return conn;
}

/* 释放连接，归回连接池 */
bool sqlconnection_pool::ReleaseConnection(MYSQL* con)
{
    if(NULL == con)
        return false;

    lock.lock();

    connList.push_back(con);
    ++m_FreeConn;
    --m_CurConn;

    lock.unlock();

    // 唤醒等待连接的线程
    reserve.post();
    return true;
}

/* 销毁数据库连接池 */
void sqlconnection_pool::DestroyPool()
{
    lock.lock();
    if(connList.size() > 0)
    {
        for(auto it = connList.begin(); it != connList.end(); ++it)
        {
            MYSQL *con = *it;
            mysql_close(con);
        }
        m_CurConn = 0;
        m_FreeConn = 0;
        connList.clear();
    }
    lock.unlock();
}

sqlconnection_pool::~sqlconnection_pool()
{
    DestroyPool();
}

int sqlconnection_pool::GetFreeConn()
{
    return m_FreeConn;
}

/*  mysql连接的RAII类实现， 创建实例时自动获得mysql连接， 销毁实例时自动归还连接给连接池 */
sqlconnectionRAII::sqlconnectionRAII(MYSQL** con, sqlconnection_pool* pool)
{
    *con = pool->GetConnection();
    conRAII = *con;
    poolRAII = pool;
}

sqlconnectionRAII::~sqlconnectionRAII()
{
    poolRAII->ReleaseConnection(conRAII);
}
