#include "MySQL_Pool.h"

namespace Sql
{
    // 单例外部构造
    MySQLPool* MySQLPool::GetConnectionPool()
    {
        Utils::Out::Out_Msg("准备构造连接池");

        // 静态构造
        static MySQLPool pool;

        Utils::Out::Out_Msg("构造连接池成功");

        return &pool;
    }

    // 构造函数
    MySQLPool::MySQLPool()
    {
        Utils::Out::Out_Msg("正在构造连接池");
        // 初始化连接数
        connectioncnt = 0;
        // 初始化停止变量
        stop = false;
    }

    // 析构函数
    MySQLPool::~MySQLPool() noexcept(false)
    {
        // 正常情况下，ShutDownMySQL() 已显式调用 shutdown() 完成清理，
        // 这里只做防御性静默清理，避免重复日志和重复 join。

        if (MoniterThread.joinable())
        {
            // 极端情况：从未调用过 shutdown()，需要停止监控线程
            {
                std::lock_guard<std::mutex> lock(queuemutex);
                stop = true;
            }
            monitor_cv.notify_all();
            MoniterThread.join();
        }

        // 静默清理残留连接
        std::lock_guard<std::mutex> lock(queuemutex);
        while (!connectionque.empty())
        {
            Connection* conn = connectionque.front();
            connectionque.pop();
            delete conn;
            connectioncnt--;
        }

        // 注意：不在析构中打印任何日志，避免出现在 "MySQL微服务已退出" 之后
    }

    // 关闭连接
    void MySQLPool::CloseConnection(Connection* conn)
    {
        Utils::Out::Out_Msg("关闭连接中");

        if (!conn)
        {
            Utils::Out::Out_Err("关闭失败");

            return;
        }

        // 删除指针
        delete conn;

        // 更新计数
        connectioncnt--;

        Utils::Out::Out_Msg("关闭连接成功，当前剩余连接数" + std::to_string(connectioncnt));
    }

    bool MySQLPool::init(const std::string ip, unsigned short port, const std::string user, const std::string password,
                         const std::string db, const int connectsize_init, const int connectsize_max,
                         const int connect_timemax, const int connect_timeout, const size_t& wait_queue_max)
    {
        Utils::Out::Out_Msg("初始化连接池中...");

        // 赋MySQL服务器地址
        this->ip = ip;
        // 赋MySQL服务器端口
        this->port = port;
        // 赋MySQL用户
        this->user = user;
        // 赋MySQL用户密码
        this->password = password;
        // 赋MySQL要连接的数据库
        this->db = db;

        // 赋初始连接数
        this->connectsize_init = connectsize_init;
        // 赋最大连接数
        this->connectsize_max = connectsize_max;
        // 赋最大空闲连接时间
        this->connect_timemax = connect_timemax;
        // 赋超时时间
        this->connect_timeout = connect_timeout;
        // 限流数
        this->wait_queue_max = wait_queue_max;

        Utils::Out::Out_Msg("连接池初始化完成");

        // 预创建连接
        {
            Utils::Out::Out_Msg("准备创建MySQL连接");

            // 智能锁
            std::lock_guard<std::mutex> lock(queuemutex);

            // 累计重复创建
            bool DoubleCreate = false;

            // 循环创建连接
            for (int i = 0; i < this->connectsize_init; i++)
            {
                // 创建连接
                Connection* conn = CreateConnection(false);

                // 判断是否创建成功
                if (conn)
                {
                    // 放入连接队列
                    connectionque.push(conn);

                    Utils::Out::Out_Msg("连接成功，当前连接数" + std::to_string(connectioncnt));

                    // 重置创建
                    DoubleCreate = false;
                }
                else
                {
                    // 判断是否已经尝试过了
                    if (!DoubleCreate)
                    {
                        Utils::Out::Out_Err("创建失败，准备重试一次");
                        // 没有则此次的i回退
                        i--;

                        DoubleCreate = true;
                    }
                    else
                    {
                        Utils::Out::Out_Err("创建失败，请重试");

                        return false;
                    }
                }
            }
        }

        // 更新停止状态
        stop = false;

        Utils::Out::Out_Msg("连接池创建完成，准备创建监控线程");

        // 创建监控线程
        MoniterThread = std::thread(&MySQLPool::MonitorLoop, this);

        Utils::Out::Out_Msg("监控线程创建成功，连接池实例创建结束");

        return true;
    }

    // 创建连接
    Connection* MySQLPool::CreateConnection(bool TempConnect)
    {
        Utils::Out::Out_Msg("创建连接中");

        // 创建连接
        Connection* connect = new Connection(TempConnect);

        // 初始化句柄
        if (!connect->Connect(ip, port, user, password, db, connect_timeout))
        {
            Utils::Out::Out_Msg("连接失败，删除句柄");

            // 连接失败，关闭连接
            delete connect;

            return nullptr;
        }

        Utils::Out::Out_Msg("创建连接成功，检查创建是否正确");

        // 查看句柄是否创建成功
        if (!connect->GetConn())
        {
            Utils::Out::Out_Msg("创建句柄失败，删除句柄");

            // 创建失败删除句柄
            delete connect;

            return nullptr;
        }

        Utils::Out::Out_Msg("创建正确，创建连接结束");

        // 连接数自增
        connectioncnt++;

        return connect;
    }

    // 获取连接
    Connection* MySQLPool::GetConnection()
    {
        Utils::Out::Out_Msg("正在获取连接");

        // 创建智能锁
        std::unique_lock<std::mutex> lock(queuemutex);

        Utils::Out::Out_Msg("检查是否需要额外创建连接");

        // 如果队列为空且小于最大连接数
        if (connectionque.empty() && connectioncnt < connectsize_max)
        {
            Utils::Out::Out_Msg("需要额外创建连接");
            // 新建连接
            Connection* conn = CreateConnection(true);

            if (conn)
            {
                Utils::Out::Out_Msg("获取到连接！");

                return conn;
            }
        }

        // 如果连接队列有归还的句柄
        if (!connectionque.empty())
        {
            Utils::Out::Out_Msg("当前队列有剩余连接！");

            // 获取conn句柄
            Connection* conn = connectionque.front();
            // 弹出队首
            connectionque.pop();

            Utils::Out::Out_Msg("获取到连接！");

            return conn;
        }

        return nullptr;
    }

    // 还连接
    void MySQLPool::ReConnection(Connection* conn)
    {
        Utils::Out::Out_Msg("校验归还的连接");

        // 校验空连接
        if (!conn)
        {
            Utils::Out::Out_Err("归还了空连接");
            return;
        }

        // 判断传入参数的正确性
        if (!conn->Ping())
        {
            Utils::Out::Out_Msg("归还的连接错误，准备删除重新创建");

            // 删除此连接并重新创建
            bool TempConnect = conn->GetTempConnect();
            delete conn;
            conn = CreateConnection(TempConnect);

            if (!conn)
            {
                Utils::Out::Out_Msg("归还的连接，重连失败");
                // 重连失败，直接放弃
                return;
            }
        }

        // 检查是否有等待者
        WaitCallback cb;
        {
            Utils::Out::Out_Msg("检查到有连接的等待，优先给他连接");

            std::lock_guard<std::mutex> lock(queuemutex);
            if (!wait_queue.empty())
            {
                cb = std::move(wait_queue.front());
                wait_queue.pop();
            }
        }

        if (cb)
        {
            // 连接所有权转移给等待者，不再入池
            //  cb 内部用完必须自己再调 ReConnection
            cb(conn);
            // 注意：到这里连接已经被拿走，不能再做任何 conn 操作
        }
        else
        {
            Utils::Out::Out_Msg("没有等待的消息，归还连接到连接池");

            // 正常入池
            std::lock_guard<std::mutex> lock(queuemutex);
            conn->SetUpdateLastTime();
            connectionque.push(conn);
            queuecv.notify_one();
        }
    }

    // 监控线程
    void MySQLPool::MonitorLoop()
    {
        // 整个循环用一个 unique_lock 持有互斥锁
        // 注意：wait_for 在等待期间会自动释放锁，唤醒/超时后重新获取锁
        std::unique_lock<std::mutex> lock(queuemutex);

        while (!stop)
        {
            // 巡检间隔：5~60秒之间
            int interval = std::max(5, std::min(60, connect_timemax / 3));

            // 关键：用 wait_for 替代 sleep_for
            // 1. 自动释放锁 → 睡眠 interval 秒（或被 notify_all 提前唤醒）
            // 2. 唤醒/超时后重新获取锁 → 检查 stop，若为 true 则返回，否则继续循环
            monitor_cv.wait_for(lock, std::chrono::seconds(interval), [this]() { return stop.load(); });

            // 被 shutdown() 唤醒后，立即退出
            if (stop)
                break;

            size_t size = connectionque.size();

            Utils::Out::Out_Msg("开始检查是否有已经空闲的额外连接！");

            // 此时已持有锁，直接操作 connectionque（和原来行为一致）
            for (size_t i = 0; i < size; i++)
            {
                // 取连接
                Connection* conn = connectionque.front();
                connectionque.pop();

                // 判断是否回收
                if (conn->GetTempConnect() && connect_timemax < conn->GetLastSeconds())
                {
                    Utils::Out::Out_Msg("检查到一个空闲的额外队列！");

                    CloseConnection(conn);

                    continue;
                }
                // 放入队列
                connectionque.push(conn);
            }

            Utils::Out::Out_Msg("检查完成，等待下次检查！");
        }
    }

    // 请求停止监控线程并立即唤醒
    void MySQLPool::StopMonitor()
    {
        Utils::Out::Out_Msg("请求停止监控线程");

        {
            // 锁内设置停止标志
            std::lock_guard<std::mutex> lock(queuemutex);
            stop = true;
        }

        // 释放锁后再唤醒，避免 MonitorLoop 刚醒又立刻阻塞
        // notify_all 会让 wait_for 立即返回（即使还没到 interval 时间）
        monitor_cv.notify_all();
    }

    // 关闭函数
    bool MySQLPool::shutdown()
    {
        Utils::Out::Out_Msg("关闭连接池中");

        Utils::Out::Out_Msg("停止监控线程");

        // 设置停止标志并立即唤醒监控线程
        StopMonitor();

        // 等待监控线程退出
        if (MoniterThread.joinable())
        {
            MoniterThread.join();
        }

        Utils::Out::Out_Msg("停止监控线程完成，准备关闭所有空闲连接");

        // 智能互斥锁
        std::lock_guard<std::mutex> lock(queuemutex);

        // 关闭所有空闲连接
        while (!connectionque.empty())
        {
            // 取出队首
            Connection* conn = connectionque.front();
            // 弹出队首
            connectionque.pop();

            // 调用关闭函数
            CloseConnection(conn);
        }
        if (connectioncnt != 0)
        {
            Utils::Out::Out_Err("关闭时可能有错误");
            return false;
        }

        Utils::Out::Out_Msg("连接池已关闭，可重新init或者关闭程序");

        return true;
    }

    // 排队等待连接
    bool MySQLPool::WaitForConnection(WaitCallback cb)
    {
        Utils::Out::Out_Msg("进入队列，准备检查是否队列已满");

        std::lock_guard<std::mutex> lock(queuemutex);

        // 限流：满了拒绝
        if (wait_queue.size() >= wait_queue_max)
        {

            Utils::Out::Out_Msg("队列已满！返回等待信息");

            // 队列已满返回false
            return false;
        }

        Utils::Out::Out_Msg("队列未满，放入队列，准备处理");

        wait_queue.push(std::move(cb));

        return true;
    }

    // 等待队列长度
    size_t MySQLPool::WaitQueueSize()
    {
        std::lock_guard<std::mutex> lock(queuemutex);

        return wait_queue.size();
    }
} // namespace Sql
