// ConnectionPool.cpp
// MySQL 连接池实现

#include "ConnectionPool.h"
#include <algorithm>

namespace MySQL {

// ===== 构造函数 =====
ConnectionPool::ConnectionPool(const PoolConfig& config)
    : m_config(config) {
    
    Utils::Out_System_Mysql("初始化连接池: 最小=" + std::to_string(config.minSize) +
                            " 最大=" + std::to_string(config.maxSize));
    
    // 创建最小数量的初始连接
    for (int i = 0; i < config.minSize; ++i) {
        MYSQL* conn = CreateNewConnection();
        if (conn) {
            auto wrapper = std::make_unique<ConnectionWrapper>();
            wrapper->conn = conn;
            wrapper->lastUsedTime = std::chrono::steady_clock::now();
            wrapper->inUse = false;
            
            m_allConnections.push_back(std::move(wrapper));
            m_idleConnections.push(conn);
            m_idleCount++;
            m_totalCount++;
        }
    }
    
    Utils::Out_System_Mysql("连接池初始化完成，初始连接数: " + std::to_string(m_totalCount.load()));
}

// ===== 析构函数 =====
ConnectionPool::~ConnectionPool() {
    m_running = false;
    m_cv.notify_all();
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // 清空空闲队列
    while (!m_idleConnections.empty()) {
        m_idleConnections.pop();
    }
    
    // 所有连接由 unique_ptr 自动释放
    m_allConnections.clear();
    
    m_activeCount = 0;
    m_idleCount = 0;
    m_totalCount = 0;
    
    Utils::Out_System_Mysql("连接池已销毁");
}

// ===== 创建新连接 =====
MYSQL* ConnectionPool::CreateNewConnection() {
    MYSQL* conn = mysql_init(nullptr);
    if (!conn) {
        Utils::Out_System_Error("mysql_init() 失败");
        return nullptr;
    }
    
    // 设置自动重连
    if (m_config.autoReconnect) {
        my_bool reconnect = 1;
        mysql_options(conn, MYSQL_OPT_RECONNECT, &reconnect);
    }
    
    // 设置连接超时
    int timeout = 5;
    mysql_options(conn, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);
    
    // 连接到服务器
    if (mysql_real_connect(conn, m_config.host.c_str(), 
                           m_config.user.c_str(), m_config.password.c_str(),
                           nullptr, m_config.port, nullptr, 0) == nullptr) {
        Utils::Out_System_Error("创建连接失败: " + std::string(mysql_error(conn)));
        mysql_close(conn);
        return nullptr;
    }
    
    // 设置字符集
    if (mysql_set_character_set(conn, "utf8mb4") != 0) {
        Utils::Out_System_Error("设置字符集失败: " + std::string(mysql_error(conn)));
    }
    
    // 选择数据库
    if (!m_config.database.empty()) {
        if (mysql_select_db(conn, m_config.database.c_str())) {
            Utils::Out_System_Error("选择数据库失败: " + std::string(mysql_error(conn)));
        }
    }
    
    Utils::Out_System_Mysql("创建新连接成功");
    return conn;
}

// ===== 验证连接有效性 =====
bool ConnectionPool::ValidateConnection(MYSQL* conn) {
    if (!conn) return false;
    return PingConnection(conn);
}

// ===== Ping 连接 =====
bool ConnectionPool::PingConnection(MYSQL* conn) {
    return mysql_ping(conn) == 0;
}

// ===== 关闭连接 =====
void ConnectionPool::CloseConnection(MYSQL* conn) {
    if (conn) {
        mysql_close(conn);
    }
}

// ===== 获取连接 =====
MYSQL* ConnectionPool::GetConnection() {
    std::unique_lock<std::mutex> lock(m_mutex);
    
    auto timeout = std::chrono::seconds(m_config.timeoutSeconds);
    auto deadline = std::chrono::steady_clock::now() + timeout;
    
    while (m_running) {
        // 1. 先检查空闲队列
        while (!m_idleConnections.empty()) {
            MYSQL* conn = m_idleConnections.front();
            m_idleConnections.pop();
            m_idleCount--;
            
            // 验证连接是否有效
            if (ValidateConnection(conn)) {
                // 标记为使用中
                for (auto& wrapper : m_allConnections) {
                    if (wrapper->conn == conn) {
                        wrapper->inUse = true;
                        wrapper->lastUsedTime = std::chrono::steady_clock::now();
                        break;
                    }
                }
                m_activeCount++;
                return conn;
            } else {
                // 连接无效，移除并创建新连接替代
                Utils::Out_System_Mysql("发现无效连接，正在移除...");
                auto it = std::find_if(m_allConnections.begin(), m_allConnections.end(),
                    [conn](const auto& wrapper) { return wrapper->conn == conn; });
                if (it != m_allConnections.end()) {
                    m_allConnections.erase(it);
                }
                CloseConnection(conn);
                m_totalCount--;
            }
        }
        
        // 2. 如果还有空闲连接但被验证消耗了，继续循环
        if (!m_idleConnections.empty()) continue;
        
        // 3. 尝试创建新连接（如果未达到最大限制）
        if (m_totalCount < m_config.maxSize) {
            lock.unlock();  // 创建连接可能耗时，先释放锁
            MYSQL* newConn = CreateNewConnection();
            lock.lock();
            
            if (newConn) {
                auto wrapper = std::make_unique<ConnectionWrapper>();
                wrapper->conn = newConn;
                wrapper->lastUsedTime = std::chrono::steady_clock::now();
                wrapper->inUse = true;
                
                m_allConnections.push_back(std::move(wrapper));
                m_activeCount++;
                m_totalCount++;
                return newConn;
            }
            // 创建失败，继续等待
        }
        
        // 4. 等待有连接归还或超时
        if (m_cv.wait_until(lock, deadline) == std::cv_status::timeout) {
            Utils::Out_System_Error("获取连接超时（" + std::to_string(m_config.timeoutSeconds) + "秒）");
            return nullptr;
        }
    }
    
    return nullptr;
}

// ===== 归还连接 =====
bool ConnectionPool::ReleaseConnection(MYSQL* conn) {
    if (!conn) return false;
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // 查找并更新连接状态
    for (auto& wrapper : m_allConnections) {
        if (wrapper->conn == conn) {
            wrapper->inUse = false;
            wrapper->lastUsedTime = std::chrono::steady_clock::now();
            
            m_idleConnections.push(conn);
            m_activeCount--;
            m_idleCount++;
            
            // 通知等待的线程
            m_cv.notify_one();
            return true;
        }
    }
    
    // 未找到该连接（可能已被移除），直接关闭
    Utils::Out_System_Error("归还未知连接，直接关闭");
    CloseConnection(conn);
    return false;
}

// ===== 动态调整连接池大小 =====
bool ConnectionPool::Resize(int newMaxSize) {
    if (newMaxSize < m_config.minSize) {
        Utils::Out_System_Error("新最大连接数不能小于最小连接数");
        return false;
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config.maxSize = newMaxSize;
    Utils::Out_System_Mysql("连接池最大连接数调整为: " + std::to_string(newMaxSize));
    return true;
}

// ===== 健康检查 =====
void ConnectionPool::HealthCheck() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    int checked = 0;
    int removed = 0;
    
    std::queue<MYSQL*> validConnections;
    
    while (!m_idleConnections.empty()) {
        MYSQL* conn = m_idleConnections.front();
        m_idleConnections.pop();
        m_idleCount--;
        checked++;
        
        if (ValidateConnection(conn)) {
            validConnections.push(conn);
            m_idleCount++;
        } else {
            // 移除无效连接
            auto it = std::find_if(m_allConnections.begin(), m_allConnections.end(),
                [conn](const auto& wrapper) { return wrapper->conn == conn; });
            if (it != m_allConnections.end()) {
                m_allConnections.erase(it);
            }
            CloseConnection(conn);
            m_totalCount--;
            removed++;
        }
    }
    
    m_idleConnections = std::move(validConnections);
    
    // 补充连接至最小连接数
    while (m_totalCount < m_config.minSize) {
        MYSQL* newConn = CreateNewConnection();
        if (newConn) {
            auto wrapper = std::make_unique<ConnectionWrapper>();
            wrapper->conn = newConn;
            wrapper->lastUsedTime = std::chrono::steady_clock::now();
            wrapper->inUse = false;
            
            m_allConnections.push_back(std::move(wrapper));
            m_idleConnections.push(newConn);
            m_idleCount++;
            m_totalCount++;
        } else {
            break;
        }
    }
    
    Utils::Out_System_Mysql("健康检查完成: 检查=" + std::to_string(checked) +
                            " 移除=" + std::to_string(removed) +
                            " 当前总数=" + std::to_string(m_totalCount.load()));
}

// ===== 清理空闲超时连接 =====
void ConnectionPool::CleanupIdleConnections() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto now = std::chrono::steady_clock::now();
    auto idleTimeout = std::chrono::seconds(m_config.idleTimeoutSeconds);
    
    // 只清理超出最小连接数的空闲连接
    int removableCount = m_idleCount - m_config.minSize;
    if (removableCount <= 0) return;
    
    std::queue<MYSQL*> keepConnections;
    int cleaned = 0;
    
    while (!m_idleConnections.empty() && cleaned < removableCount) {
        MYSQL* conn = m_idleConnections.front();
        m_idleConnections.pop();
        m_idleCount--;
        
        // 查找该连接的最后使用时间
        auto it = std::find_if(m_allConnections.begin(), m_allConnections.end(),
            [conn](const auto& wrapper) { return wrapper->conn == conn; });
        
        if (it != m_allConnections.end()) {
            auto age = std::chrono::duration_cast<std::chrono::seconds>(
                now - (*it)->lastUsedTime).count();
            
            if (age >= m_config.idleTimeoutSeconds && cleaned < removableCount) {
                // 关闭超时连接
                m_allConnections.erase(it);
                CloseConnection(conn);
                m_totalCount--;
                cleaned++;
            } else {
                keepConnections.push(conn);
                m_idleCount++;
            }
        } else {
            CloseConnection(conn);
            m_totalCount--;
            cleaned++;
        }
    }
    
    // 将保留的连接放回队列
    while (!keepConnections.empty()) {
        m_idleConnections.push(keepConnections.front());
        keepConnections.pop();
    }
    
    if (cleaned > 0) {
        Utils::Out_System_Mysql("清理空闲连接: " + std::to_string(cleaned) +
                                " 当前总数=" + std::to_string(m_totalCount.load()));
    }
}

} // namespace MySQL