#include "MySQL_Connection.h"

namespace Sql
{
    // 构造初始化连接
    Connection::Connection()
    {
        // 初始化连接
        conn = mysql_init(nullptr);
    }

    // 关闭连接并析构
    Connection::~Connection()
    {
        // 判断是否有连接
        if (conn != nullptr)
        {
            // 有连接，则关闭
            mysql_close(conn);
        }
    }

    bool Connection::Connect(const std::string ip, const unsigned short port, const std::string user,
                             const std::string password, const std::string db)
    {
        MYSQL* p = mysql_real_connect(conn, ip.c_str(), user.c_str(), password.c_str(), db.c_str(), port, nullptr, 0);
        return p != nullptr;
    }
} // namespace Sql