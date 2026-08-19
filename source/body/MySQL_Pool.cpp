#include "MySQL_Pool.h"

namespace Sql
{
    // 单例外部构造
    MySQLPool* MySQLPool::GetConnectionPool()
    {
        // 静态构造
        static MySQLPool pool;

        return &pool;
    }

    // 构造函数
    MySQLPool::MySQLPool()
    {
        // 初始化连接数
        connectioncnt = 0;
        // 初始化停止变量
        stop = false;
    }

    // 析构函数
    MySQLPool::~MySQLPool() noexcept(false)
    {
        Utils::Out::Out_Msg("关闭连接池中");

        // 关闭连接
        if (!shutdown())
        {
            throw std::runtime_error("连接池关闭时仍有连接被借用");
        }
    }

    // 关闭连接
    void MySQLPool::CloseConnection(Connection* conn)
    {
        if (!conn)
        {
            Utils::Out::Out_Err("关闭失败");

            return;
        }

        if (conn->GetConn())
        {
            // 连接关闭
            mysql_close(conn->GetConn());
        }

        // 删除指针
        delete conn;

        // 更新计数
        connectioncnt--;
    }

    bool MySQLPool::init(const std::string ip, unsigned short port, const std::string user, const std::string password,
                         const std::string db, const int connectsize_init, const int connectsize_max,
                         const int connect_timemax, const int connect_timeout)
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

                    // 连接数自增
                    connectioncnt++;

                    // 重置创建
                    DoubleCreate = false;
                }
                else
                {
                    // 判断是否已经尝试过了
                    if (!DoubleCreate)
                    {
                        // 没有则此次的i回退
                        i--;
                    }
                    else
                    {
                        Utils::Out::Out_Err("创建失败，准备重试");

                        return false;
                    }
                }
            }
        }

        // 更新停止状态
        stop = false;

        // 创建监控线程
        MoniterThread = std::thread(&MySQLPool::MonitorLoop, this);

        return true;
    }

    // 创建连接
    Connection* MySQLPool::CreateConnection(bool TempConnect)
    {
        // 创建连接
        Connection* connect = new Connection(TempConnect);

        // 查看句柄是否创建成功
        if (!connect->GetConn())
        {
            // 创建失败删除句柄
            delete connect;

            return nullptr;
        }

        //
        if (!connect->Connect(ip, port, user, password, db, connect_timeout))
        {
            // 连接失败，关闭连接
            delete connect;
            return nullptr;
        }

        return connect;
    }

    // 获取连接
    Connection* MySQLPool::GetConnection()
    {
        Utils::Out::Out_Msg("正在获取连接");

        // 创建智能锁
        std::unique_lock<std::mutex> lock(queuemutex);

        // 如果队列不为空
        if (connectionque.empty() && connectioncnt < connectsize_max)
        {
            // 新建连接
            Connection* conn = CreateConnection(true);

            if (conn)
            {
                connectioncnt++;

                Utils::Out::Out_Msg("获取到连接！");

                return conn;
            }
        }

        // 等待连接
        queuecv.wait_for(lock, std::chrono::seconds(connect_timeout), [this] { return !connectionque.empty(); });

        // 如果连接队列有句柄
        if (!connectionque.empty())
        {
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
        // 判断传入参数的正确性
        if (!conn)
        {
            Utils::Out::Out_Msg("conn传入错误");

            return;
        }

        // 判断是否
        if (!conn->Ping())
        {
            Utils::Out::Out_Msg("conn连接断开，尝试重连");

            // 获取删除的类型
            bool TempConnect = conn->GetTempConnect();

            // 删除指针
            delete conn;

            // 重新创建新的连接
            conn = CreateConnection(TempConnect);
        }

        {
            // 智能互斥锁
            std::lock_guard<std::mutex> lock(queuemutex);

            // 设置最后一次的时间
            conn->SetUpdateLastTime();

            // 放入队列
            connectionque.push(conn);
        }

        // 唤醒一个等待线程
        queuecv.notify_one();
    }

    // 监控线程
    void MySQLPool::MonitorLoop()
    {
        while (!stop)
        {
            // 巡检间隔
            int interval = std::max(5, std::min(60, connect_timemax / 3));
            std::this_thread::sleep_for(std::chrono::seconds(interval));

            // 智能互斥锁
            std::lock_guard<std::mutex> lock(queuemutex);
            size_t size = connectionque.size();

            for (size_t i = 0; i < size; i++)
            {
                // 取连接
                Connection* conn = connectionque.front();
                connectionque.pop();

                // 判断是否回收
                if (conn->GetTempConnect() && connect_timemax < conn->GetLastSeconds())
                {
                    CloseConnection(conn);
                }
                // 放入队列
                connectionque.push(conn);
            }
        }
    }

    // 关闭函数
    bool MySQLPool::shutdown()
    {
        Utils::Out::Out_Msg("关闭连接池中");

        // 停止监控线程
        stop = true;

        // 等待监控线程退出
        if (MoniterThread.joinable())
        {
            MoniterThread.join();
        }

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

        Utils::Out::Out_Msg("连接池已关闭，可重新init");

        return true;
    }
} // namespace Sql
