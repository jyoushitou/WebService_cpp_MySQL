// 冒烟测试：验证“头文件 + 链接库（.lib/.so）”这一层可以正常使用。
// 不连接真实数据库，因此可以离线运行。
#include "MySQL_Pool.h"
#include "MySQL_Connection.h"

#include <cstdio>

namespace
{
    int Fail(const char* what)
    {
        std::printf("dbpool smoke test FAILED: %s\n", what);
        return 1;
    }
} // namespace

int main()
{
    // 1) 单条连接对象：不连库也能构造/析构，且状态正确
    {
        Sql::Connection conn(true);

        if (conn.GetConn() != nullptr)
        {
            return Fail("new Connection should not own a handle");
        }
        if (!conn.GetTempConnect())
        {
            return Fail("TempConnect flag not preserved");
        }
        if (conn.GetLastSeconds() < 0)
        {
            return Fail("LastUsedTime not initialized");
        }
        if (conn.Ping())
        {
            return Fail("Ping() on closed connection should return false");
        }
    }

    // 2) 连接池单例：不 init 就不会启动监控线程，可以安全地验证等待队列
    Sql::MySQLPool* pool = Sql::MySQLPool::GetConnectionPool();
    if (pool == nullptr)
    {
        return Fail("GetConnectionPool() returned null");
    }
    if (pool->WaitQueueSize() != 0)
    {
        return Fail("wait queue should start empty");
    }

    bool accepted = pool->WaitForConnection([](Sql::Connection*) {});
    if (!accepted)
    {
        return Fail("WaitForConnection() rejected an empty queue request");
    }
    if (pool->WaitQueueSize() != 1)
    {
        return Fail("WaitQueueSize() did not reflect the queued callback");
    }

    if (!pool->shutdown())
    {
        return Fail("shutdown() failed on an idle pool");
    }

    std::printf("dbpool smoke test passed\n");
    return 0;
}
