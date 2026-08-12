// sql conection
#pragma once

#include <string>

#include <mysql.h>

#include "Utils.h"

namespace Sql
{
    // mysql的连接
    class Connection
    {
    public:
        // 构造初始化连接
        Connection();
        // 关闭连接并析构
        ~Connection();

        // 连接
        bool Connect(const std::string ip, const unsigned short port, const std::string user,
                     const std::string password, const std::string db);

        //
    private:
        // 保存数据库的连接
        MYSQL* conn;
    };
} // namespace Sql