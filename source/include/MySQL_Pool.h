#pragma once

#include <queue>
#include <mutex>
#include <stdexcept>

#include <mysql.h>

#include "MySQL_Connection.h"

namespace Sql
{
    // 数据库连接池
    class MySQLPool
    {
    public:
        // 获取连接池实例
        static MySQLPool* Get_ConnectionPool();

        // 公有方法：初始化连接池参数
        bool init(const std::string ip, unsigned short port, const std::string user, const std::string password,
                  const std::string db, const int initSize, const int maxSize, const int ConnectSize_init,
                  const int ConnectSize_Max, const int Connect_TimeMax, const int Connect_TimeOut, const int serviceID);

        // 获取连接
        Connection* Get_Connection();

        // 返回连接
        void Push_Connection(Connection* conn);

    private:
        // 构造函数私有化
        // 防止外部创建新实例
        MySQLPool();
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
        int ConnectSize_init;
        // 最大连接量
        int ConnectSize_Max;
        // 连接最大空闲时间
        int Connect_TimeMax;
        // 连接超时
        int Connect_TimeOut;

        // 存储连接队列
        std::queue<Connection*> ConnectionQue;
        // 存储队列数量
        int ConnectionCnt;
        // 维护连接队列的线程安全互斥锁
        std::mutex QueueMutex;

        // 服务器ID
        int serviceID;

        // 连接可用时唤醒等待线程
        std::condition_variable QueueCv;

        // 停止标志
        std::atomic<bool> stop;
        // 扫描线程
        // 关闭因保持高并发时的额外连接连接
        std::thread KillConnectionThread;

        // 记录最后的时间戳
        std::chrono::steady_clock::time_point lastTime;
    };
} // namespace Sql