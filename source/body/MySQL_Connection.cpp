#include "MySQL_Connection.h"

namespace Sql
{
    // 构造初始化连接
    Connection::Connection()
    {
        // 初始化状态
        Connecting = false;
        // 初始化conn指针
        conn = nullptr;
        // 刷新使用时间
        LastUsedTime = std::chrono::steady_clock::now();
    }

    // 关闭连接并析构
    Connection::~Connection()
    {
        // 直接关闭
        if (DisConnect())
        {
            throw std::invalid_argument("关闭失败，请重试");
        }
    }

    bool Connection::Connect(const std::string ip, const unsigned short port, const std::string user,
                             const std::string password, const std::string db, const unsigned int TimeOut)
    {
        // 初始化连接指针
        conn = mysql_init(nullptr);

        // 判断是否初始化完成
        if (!conn)
        {
            return false;
        }

        // 设置连接超时
        mysql_options(conn, MYSQL_OPT_CONNECT_TIMEOUT, &TimeOut);

        // 连接数据库
        if (!mysql_real_connect(conn, ip.c_str(), user.c_str(), password.c_str(), db.c_str(), port, nullptr, 0))
        {
            // 连接失败则关闭
            mysql_close(conn);
            conn = nullptr;
            return false;
        }

        // 设置编码模式为utf-8
        mysql_set_character_set(conn, "utf8mb4");
        // 设置连接状态
        Connecting = true;
        // 刷新时间
        UpdateLastConnected();

        return true;
    }

    bool Connection::DisConnect()
    {
        if (conn)
        {
            mysql_close(conn);
            conn = nullptr;
            Connecting = false;
        }
    }

} // namespace Sql