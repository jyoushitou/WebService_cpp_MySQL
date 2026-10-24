/// @file        MySQL_Pool.h
/// @brief       MySQL 连接池：预创建、等待队列限流、空闲回收
/// @author      jyoushitou
/// @date        2026-10-24
/// @copyright   Copyright (c) 2026

// 防止重复包含
#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <chrono>
#include <functional>
#include <algorithm>
#include <string>

#include "MySQL_Connection.h"
#include <mysql.h>

/// @namespace   Sql
/// @brief      数据库相关组件作用域
/// @details    连接池与单条连接均位于此命名空间
namespace Sql
{
    /// @brief      MySQL 任务回调
    /// @details   接收一条已借出的连接，返回需要回给调用方的字符串
    /// @note       连接池不关心业务，回调由调用方实现
    using MySQLTask = std::function<std::string(Sql::Connection*)>;

    /// @class       MySQLPool
    /// @brief       数据库连接池（懒汉单例）
    /// @details     启动时预创建连接；连接耗尽时排队等待；监控线程定期回收空闲连接
    /// @note        实例唯一，通过 GetConnectionPool() 获取，禁止外部构造
    /// @warning     使用完必须调用 shutdown()，否则监控线程不会退出
    class MySQLPool
    {
    public:
        /// @brief      获取连接池实例
        /// @details    首次调用时构造唯一实例，之后直接返回
        /// @return     连接池指针，保证非空
        /// @note       静态局部变量，C++11 起构造线程安全
        static MySQLPool* GetConnectionPool();

        /// @brief      初始化连接池
        /// @details    保存连接参数、预创建初始连接并启动监控线程
        /// @param[in]  ip               数据库地址
        /// @param[in]  port             数据库端口
        /// @param[in]  user             数据库账号
        /// @param[in]  password         数据库密码
        /// @param[in]  db               连接的数据库名
        /// @param[in]  connectsize_init 预创建的连接数
        /// @param[in]  connectsize_max  最大连接数
        /// @param[in]  connect_timemax  连接最大空闲时间（秒）
        /// @param[in]  connect_timeout  连接超时（秒）
        /// @param[in]  wait_queue_max   等待队列上限（限流）
        /// @return     true 初始化成功；false 预创建连接失败
        /// @warning    重复调用会重新预创建连接，必须先 shutdown()
        bool init(const std::string ip, unsigned short port, const std::string user, const std::string password,
                  const std::string db, const int connectsize_init, const int connectsize_max,
                  const int connect_timemax, const int connect_timeout, const size_t& wait_queue_max);

        /// @brief      关闭连接池
        /// @details    停止监控线程，并关闭池内所有空闲连接
        /// @return     true 关闭成功；false 仍有连接未归还
        /// @warning    关闭后池不可再用，重新使用前需再次 init()
        bool shutdown();

        /// @brief      等待连接的回调类型
        /// @details    通用回调，不依赖任何业务；拿到连接后由回调自行负责归还
        using WaitCallback = std::function<void(Connection*)>;

        /// @brief      排队等待连接
        /// @details    连接耗尽时把回调放入等待队列，有连接归还时优先交给它
        /// @param[in]  cb 拿到连接后的回调，回调内部用完必须调用 ReConnection()
        /// @return     true 已入队；false 队列已满，被限流拒绝
        /// @warning    回调中不能再阻塞等待连接，否则可能死锁
        bool WaitForConnection(WaitCallback cb);

        /// @brief      获取当前等待队列长度
        /// @return     队列中的回调数量
        /// @note       主要用于监控与限流判断
        size_t WaitQueueSize();

        /// @brief      获取一条连接
        /// @details    优先取空闲连接；没有空闲且未达上限时临时创建
        /// @return     连接对象；池空且已达上限时返回 nullptr
        /// @warning    用完必须调用 ReConnection() 归还，否则连接会泄漏
        Connection* GetConnection();

        /// @brief      归还连接
        /// @details    先 Ping 校验；失效则销毁重建；有等待者时直接交给等待者
        /// @param[in]  conn 待归还的连接，允许为 nullptr（仅记日志）
        /// @warning    归还后不得再使用该指针，所有权已交回连接池
        void ReConnection(Connection* conn);

        /// @brief      请求停止监控线程
        /// @details    置停止标志并立即唤醒监控线程，不等待其退出
        /// @note       通常由 shutdown() 内部调用
        void StopMonitor();

    private:
        /// @brief      构造函数
        /// @details    私有化，防止外部创建新实例
        MySQLPool();

        /// @brief      析构函数
        /// @details    做防御性清理：停止监控线程、释放残留连接
        /// @warning    noexcept(false)，允许析构过程中抛出异常
        ~MySQLPool() noexcept(false);

        /// @brief      禁止拷贝构造
        MySQLPool(const MySQLPool&) = delete;

        /// @brief      禁止赋值
        MySQLPool& operator=(const MySQLPool&) = delete;

        /// @brief      创建一条连接
        /// @param[in]  TempConnect true 临时连接（可被监控线程回收）；false 常驻连接
        /// @return     创建成功的连接；失败返回 nullptr
        /// @note       成功后连接计数自增
        Connection* CreateConnection(bool TempConnect);

        /// @brief      关闭并释放一条连接
        /// @param[in]  conn 待关闭的连接
        /// @note       调用方需自行持有 queuemutex
        void CloseConnection(Connection* conn);

        /// @brief      监控线程主循环
        /// @details    按间隔巡检，回收空闲时间超过 connect_timemax 的临时连接
        /// @note       等待期间通过 monitor_cv 释放锁，可被 StopMonitor() 立即唤醒
        void MonitorLoop();

        // === 数据库连接参数 ===

        std::string ip;             ///< 服务器 ip 地址
        unsigned short port;        ///< 服务器端口
        std::string user;           ///< 用户名
        std::string password;       ///< 密码
        std::string db;             ///< 数据库名称

        int connectsize_init;       ///< 初始连接量
        int connectsize_max;        ///< 最大连接量
        int connect_timemax;        ///< 连接最大空闲时间
        int connect_timeout;        ///< 连接超时

        // === 连接队列 ===

        std::queue<Connection*> connectionque;  ///< 空闲连接队列
        int connectioncnt;                      ///< 当前连接总数（含借出的）
        std::mutex queuemutex;                  ///< 保护连接队列的互斥锁
        std::condition_variable queuecv;        ///< 连接入池时唤醒取连接的一方

        // === 监控线程 ===

        std::thread MoniterThread;              ///< 监控线程
        std::atomic<bool> stop;                 ///< 停止标志
        std::condition_variable monitor_cv;     ///< 替代裸 sleep，让 shutdown 能立即唤醒监控线程

        // === 等待队列 ===

        std::queue<WaitCallback> wait_queue;    ///< 等待连接的任务队列，只存通用回调
        size_t wait_queue_max = 1000;           ///< 等待队列上限
    };
} // namespace Sql
