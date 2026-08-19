// sql conection
#pragma once

#ifdef _WIN32
#include <winsock2.h>
#endif

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
        Connection(bool TempConnect);
        // 关闭连接并析构
        ~Connection();

        // 连接
        bool Connect(const std::string ip, const unsigned int port, const std::string user, const std::string password,
                     const std::string db, const unsigned int TimeOut);

        // 检查连接
        bool Ping();

        // 获取连接的类型
        bool GetTempConnect();
        // 获取原始句柄
        MYSQL* GetConn();
        // 获取最后的更新时间
        long long GetLastSeconds();

        // 设置最后一次连接的时间
        void SetUpdateLastTime();

        // 查询
        MYSQL_RES* Select();

        // 插入
        bool Insert();
        // 日志归档
        bool InsertLog();

        // 更新
        bool Update();

        // 删除
        bool Delete();

        // 自定义语句的函数
        // 无返回的自定义语句
        bool Query_NoReturn(const std::string sql);
        // 自定义语句的函数
        // 有返回的自定义语句
        MYSQL_RES* Query_Return(const std::string sql);

    private:
        // 关闭拷贝构造
        Connection(const Connection&) = default;
        Connection& operator=(const Connection&) = delete;

        // 关闭连接
        bool DisConnect();

        // 保存数据库的连接
        MYSQL* conn;

        // 判断连接状态
        bool Connecting;

        // 最后一次使用时间计时
        std::chrono::steady_clock::time_point LastUsedTime;

        // 判断连接类型
        bool TempConnect;
    };
} // namespace Sql