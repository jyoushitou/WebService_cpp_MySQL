// ConnectionPool.h
// MySQL 连接池 — 管理多个数据库连接，支持并发访问

#ifndef CONNECTION_POOL_H
#define CONNECTION_POOL_H

#include <iostream>
#include <string>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <atomic>
#include <chrono>
#include <thread>

// 跨平台包含 MySQL C API 头文件
#ifdef _WIN32
    #include <mysql.h>
#else
    #include <mysql/mysql.h>
#endif

#include "Utils.h"

namespace MySQL {

// ===== 连接池配置 =====
struct PoolConfig {
    std::string host = "localhost";
    int port = 3306;
    std::string user = "web_server";
    std::string password = "123456";
    std::string database = "web_server";
    
    int minSize = 2;           // 最小连接数（常驻）
    int maxSize = 10;          // 最大连接数
    int timeoutSeconds = 30;   // 获取连接超时时间（秒）
    int idleTimeoutSeconds = 300; // 空闲连接超时（秒），超过此时间未使用的连接将被关闭
    bool autoReconnect = true; // 是否自动重连

    // Windows 本地开发用 localhost，Linux 部署用远程服务器
    #ifdef _WIN32
        PoolConfig() : host("localhost") {}
    #else
        PoolConfig() : host("192.168.0.52") {}
    #endif
};

// ===== 连接包装 =====
// 封装 MYSQL* 及其元数据
struct ConnectionWrapper {
    MYSQL* conn = nullptr;
    std::chrono::steady_clock::time_point lastUsedTime;
    bool inUse = false;
    
    ConnectionWrapper() = default;
    
    ~ConnectionWrapper() {
        if (conn) {
            mysql_close(conn);
            conn = nullptr;
        }
    }
    
    // 禁止拷贝，允许移动
    ConnectionWrapper(const ConnectionWrapper&) = delete;
    ConnectionWrapper& operator=(const ConnectionWrapper&) = delete;
    ConnectionWrapper(ConnectionWrapper&& other) noexcept 
        : conn(other.conn), lastUsedTime(other.lastUsedTime), inUse(other.inUse) {
        other.conn = nullptr;
    }
    ConnectionWrapper& operator=(ConnectionWrapper&& other) noexcept {
        if (this != &other) {
            if (conn) mysql_close(conn);
            conn = other.conn;
            lastUsedTime = other.lastUsedTime;
            inUse = other.inUse;
            other.conn = nullptr;
        }
        return *this;
    }
};

// ===== 连接池类 =====
class ConnectionPool {
public:
    // 构造函数：传入配置，自动初始化连接池
    explicit ConnectionPool(const PoolConfig& config = PoolConfig());
    
    // 析构函数：关闭所有连接
    ~ConnectionPool();
    
    // 获取一个可用连接（阻塞等待，超时返回 nullptr）
    MYSQL* GetConnection();
    
    // 归还连接（使用完后必须调用）
    bool ReleaseConnection(MYSQL* conn);
    
    // 获取当前连接池状态
    int GetActiveCount() const { return m_activeCount.load(); }
    int GetIdleCount() const { return m_idleCount.load(); }
    int GetTotalCount() const { return m_totalCount.load(); }
    
    // 动态调整连接池大小
    bool Resize(int newMaxSize);
    
    // 健康检查：验证所有空闲连接是否有效
    void HealthCheck();

private:
    PoolConfig m_config;
    
    // 连接存储
    std::vector<std::unique_ptr<ConnectionWrapper>> m_allConnections;
    std::queue<MYSQL*> m_idleConnections;
    
    // 同步原语
    mutable std::mutex m_mutex;
    std::condition_variable m_cv;
    
    // 连接计数
    std::atomic<int> m_activeCount{0};
    std::atomic<int> m_idleCount{0};
    std::atomic<int> m_totalCount{0};
    
    // 运行状态
    std::atomic<bool> m_running{true};
    
    // 内部方法
    MYSQL* CreateNewConnection();
    bool ValidateConnection(MYSQL* conn);
    void CloseConnection(MYSQL* conn);
    bool PingConnection(MYSQL* conn);
    
    // 清理空闲超时连接
    void CleanupIdleConnections();
};

// ===== 连接池智能指针 =====
// RAII 风格：构造时获取连接，析构时自动归还
class PooledConnection {
public:
    PooledConnection(ConnectionPool& pool) 
        : m_pool(pool), m_conn(pool.GetConnection()) {}
    
    ~PooledConnection() {
        if (m_conn) {
            m_pool.ReleaseConnection(m_conn);
        }
    }
    
    // 获取原始 MYSQL* 指针
    MYSQL* Get() const { return m_conn; }
    
    // 检查是否成功获取连接
    bool IsValid() const { return m_conn != nullptr; }
    
    // 禁止拷贝
    PooledConnection(const PooledConnection&) = delete;
    PooledConnection& operator=(const PooledConnection&) = delete;
    
    // 允许移动
    PooledConnection(PooledConnection&& other) noexcept
        : m_pool(other.m_pool), m_conn(other.m_conn) {
        other.m_conn = nullptr;
    }
    
private:
    ConnectionPool& m_pool;
    MYSQL* m_conn;
};

} // namespace MySQL

#endif // !CONNECTION_POOL_H