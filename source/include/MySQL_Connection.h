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
        // 构造初始化连接
        Connection();
        // 关闭连接并析构
        ~Connection();

        // 连接
        bool Connect(const std::string ip, const unsigned short port, const std::string user,
                     const std::string password, const std::string db, const unsigned int TimeOut);

        // 关闭连接
        bool DisConnect();

        // 获取句柄
        MYSQL* GetConn();

        // 是否连接
        bool Connected();

        // 更新最后修改的时间戳
        void UpdateLastConnected();
        // 获取空闲时间
        unsigned long long GetSecond();

        // 自定义函数
        bool Query(const std::string);

    private:
        // 保存数据库的连接
        MYSQL* conn;
        // 判断连接状态
        bool Connecting;
        // 最后一次使用时间计时
        std::chrono::steady_clock::time_point LastUsedTime;
    };
} // namespace Sql