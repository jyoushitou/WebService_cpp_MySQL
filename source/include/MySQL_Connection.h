// sql conection
#pragma once

#include <string>
#include <chrono>
#include <exception>

#include <mysql.h>

#include "Utils.h"

namespace Sql
{
    // mysql的连接
    class Connection
    {
    public:
        // 构造初始化
        Connection(int serviceID, bool TempConnect);
        // 关闭连接并析构
        ~Connection();

        // 连接
        bool Connect(const std::string ip, const unsigned int port, const std::string user, const std::string password,
                     const std::string db, const unsigned int TimeOut);

        // 关闭连接
        bool DisConnect();

        // 检查连接
        bool Ping();

        // 获取原始句柄
        MYSQL* GetConn();

        // 自定义语句的函数
        // 无返回的自定义语句
        bool Query_NoReturn(const std::string sql);
        // 自定义语句的函数
        // 有返回的自定义语句
        MYSQL_RES* Query_Return(const std::string sql);

    private:
        // 保存数据库的连接
        MYSQL* conn;

        // 判断连接状态
        bool Connecting;

        // 最后一次使用时间计时
        std::chrono::steady_clock::time_point LastUsedTime;

        // 判断连接类型
        bool TempConnect;

        // 服务器ID
        int serviceID;
    };
} // namespace Sql