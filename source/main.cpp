#include "MySQL_Pool.h"
#include "ThreadPool.h"

int main()
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

    for (int i = 0; i < 18; ++i)
    {
        tp.enqueue(
            [pool, i]
            {
                auto* conn = pool->GetConnection();
                if (conn)
                {
                    mysql_query(conn->GetMysql(), "SELECT ...");
                    pool->ReleaseConnection(conn);
                }
            });
    }
    return 0;
}