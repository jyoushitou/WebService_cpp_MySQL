#include "work.h"

void StartMySQL(const std::string ip, unsigned short port, const std::string user, const std::string password,
                const std::string db, const int initSize, const int maxSize, const int connectsize_init,
                const int connectsize_max, const int connect_timemax, const int connect_timeout, const int serviceID)
{
    auto* pool = Sql::MySQLPool::GetConnectionPool();
    pool->init("127.0.0.1", 3306, "root", "123456", "mydb",
               6,   // initSize（此参数已闲置，可填任意值）
               18,  // maxSize（同上）
               6,   // ConnectSize_init 初始连接：永不回收
               18,  // ConnectSize_Max 最大连接：动态不能超过此数
               300, // Connect_TimeMax 动态连接空闲秒数
               5,   // Connect_TimeOut 取连接超时秒数
               1);  // serviceID

    ThreadPool::ThreadPool tp(6); // 6个工作线程
}