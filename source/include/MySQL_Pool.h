// sql Pool
#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <chrono>
#include <functional>

#include "MySQL_Connection.h"
#include <mysql.h>

namespace Sql
{
    // 数据库连接池
    class MySQLPool
    {
    public:
        // 获取连接池实例
        static MySQLPool* GetConnectionPool();

        // 公有方法：初始化连接池参数
        bool init(const std::string ip, unsigned short port, const std::string user, const std::string password,
                  const std::string db, const int connectsize_init, const int connectsize_max,
                  const int connect_timemax, const int connect_timeout);

        // 关闭已有连接
        bool shutdown();

        // 获取连接
        Connection* GetConnection();

        // 返回连接
        void ReConnection(Connection* conn);

    private:
        // 构造函数私有化
        // 防止外部创建新实例
        MySQLPool();

        // 析构函数（noexcept(false) 允许析构中抛出异常）
        ~MySQLPool() noexcept(false);

        // 取消传递构造/拷贝构造
        MySQLPool(const MySQLPool&) = delete;
        MySQLPool& operator=(const MySQLPool&) = delete;

        // 创建连接
        Connection* CreateConnection(bool TempConnect);
        // 关闭连接
        void CloseConnection(Connection* conn);

        // 监控线程主循环
        void MonitorLoop();

        // ===类内数据库变量===

        // 服务器ip地址
        std::string ip;
        // 服务器端口
        unsigned short port;
        // 用户名
        std::string user;
        // 密码
        std::string password;
        // 数据库名称
        std::string db;

        // 初始连接量
        int connectsize_init;
        // 最大连接量
        int connectsize_max;
        // 连接最大空闲时间
        int connect_timemax;
        // 连接超时
        int connect_timeout;

        // ===存储消息队列===

        // 存储连接队列
        std::queue<Connection*> connectionque;
        // 存储队列里的数量
        int connectioncnt;
        // 维护连接队列的安全互斥锁
        std::mutex queuemutex;
        // 连接不足时阻塞等待
        std::condition_variable queuecv;

        // ===监控线程===
        // 监控线程
        std::thread MoniterThread;
        // 停止标志
        std::atomic<bool> stop;
    };

    // MySQL专属任务函数类型
    using MySQLTask = std::function<void(Connection*)>;
} // namespace Sql