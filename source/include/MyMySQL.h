// MyMySQL.h
// 借助MySQL C API实现通讯MySQL
// 集成连接池，支持高并发访问

#ifndef MYMYSQL_H
#define MYMYSQL_H
#include <iostream>         // 输入输出流
#include <string>           // 字符串
// 跨平台包含 MySQL C API 头文件
#ifdef _WIN32
    #include <mysql.h>           // Windows：CMake 已添加 include 目录，直接引用
#else
    #include <mysql/mysql.h>     // Linux：系统路径 /usr/include/mysql/mysql.h
#endif

#include "Utils.h"          // 工具库（日志输出）
#include "ConnectionPool.h" // 连接池

namespace MySQL{
    bool Create_DataBases(MYSQL* ,std::string);     // 全局函数：检查并创建数据库（接受外部的 MYSQL* 句柄）

    // ===== 连接池管理器（单例） =====
    // 全局唯一的连接池实例，所有 mysql 对象共享
    class PoolManager {
    public:
        static PoolManager& Instance() {
            static PoolManager instance;
            return instance;
        }

        // 初始化连接池（首次调用时配置生效）
        void Initialize(const PoolConfig& config = PoolConfig()) {
            std::call_once(m_initFlag, [this, &config]() {
                m_pool = std::make_unique<ConnectionPool>(config);
                Utils::Out_System_Mysql("连接池管理器初始化完成");
            });
        }

        // 获取连接池引用
        ConnectionPool& GetPool() {
            if (!m_pool) {
                Initialize();  // 默认初始化
            }
            return *m_pool;
        }

        // 获取连接（RAII 包装）
        PooledConnection GetConnection() {
            return PooledConnection(GetPool());
        }

        // 健康检查
        void HealthCheck() {
            if (m_pool) {
                m_pool->HealthCheck();
            }
        }

    private:
        PoolManager() = default;
        ~PoolManager() = default;
        PoolManager(const PoolManager&) = delete;
        PoolManager& operator=(const PoolManager&) = delete;

        std::unique_ptr<ConnectionPool> m_pool;
        std::once_flag m_initFlag;
    };

    // ===== mysql 类 — 数据库连接和操作封装 =====
    // 封装了一个完整的 MySQL 连接生命周期：
    // 内部使用连接池管理连接，支持多线程并发访问
    class mysql{
        public:
            //构造函数
            mysql();                                             // 默认构造函数：连接 \"web_server\" 数据库
            mysql(std::string str);                              // 指定数据库名的构造函数

            // 数据库管理
            bool Create_DataBases();                             // 创建/验证当前默认数据库
            bool Create_DataBases(std::string);                  // 创建/验证指定数据库并设为默认

            //外部可使用工具
            int User(std::string, std::string, std::string* userAvatar = nullptr);      // 用户名+密码鉴权，返回权限级别（0=无权限，>0=有权限），可选输出头像
            int Get_UserId(std::string);                                                // 根据用户名查询用户 ID
            std::string Get_Avatar(std::string);                                        // 根据用户名查询用户头像

            // 连接池管理
            static void InitPool(const PoolConfig& config = PoolConfig()) {
                PoolManager::Instance().Initialize(config);
            }
            static void PoolHealthCheck() {
                PoolManager::Instance().HealthCheck();
            }

            ~mysql();                                            // 析构时自动关闭连接

        private:
            //参数
            std::string database;                                // 当前选中的数据库名称

            // 从连接池获取连接
            MYSQL* GetConnection() {
                return PoolManager::Instance().GetPool().GetConnection();
            }

            // 归还连接到连接池
            bool ReleaseConnection(MYSQL* conn) {
                return PoolManager::Instance().GetPool().ReleaseConnection(conn);
            }

            //内部工具
            bool Create_Socket();                                // 检查句柄是否创建成功（兼容旧接口）
            bool Connect();                                      // 连接到 MySQL 服务器（兼容旧接口）
            bool Close();                                        // 关闭数据库连接（兼容旧接口）

            bool Set_UTF8();                                     // 设置字符集为 utf8mb4（支持中文和 emoji）
            bool Set_Databases(std::string);                     // 切换到指定数据库

    };
}
#endif //!MYMYSQL_H