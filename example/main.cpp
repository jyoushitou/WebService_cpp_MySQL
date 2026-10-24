/// @file        main.cpp
/// @brief       dbpool 独立演示程序：初始化连接池 -> 查询 -> 归还连接 -> 关闭
/// @author      jyoushitou
/// @date        2026-10-24
/// @copyright   Copyright (c) 2026

#include "MySQL_Pool.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

/// @brief       演示程序内部实现
namespace
{
    /// @brief      打印用法
    /// @param[in]  exe 可执行文件路径
    void PrintUsage(const char* exe)
    {
        std::printf(
            "用法: %s [ip] [port] [user] [password] [db]\n"
            "示例: %s 127.0.0.1 3306 root 123456 test\n"
            "环境变量: MYSQL_HOST / MYSQL_PORT / MYSQL_USER / MYSQL_PASSWORD / MYSQL_DB\n",
            exe, exe);
    }

    /// @brief      读取环境变量，为空时返回默认值
    /// @param[in]  key      环境变量名
    /// @param[in]  fallback 默认值
    /// @return     环境变量值或默认值
    std::string EnvOr(const char* key, const char* fallback)
    {
        const char* value = std::getenv(key);
        return (value != nullptr && *value != '\0') ? std::string(value) : std::string(fallback);
    }

    /// @brief      字符串转整数
    /// @param[in]  text     待转换文本，可为 nullptr
    /// @param[in]  fallback 文本为空时的默认值
    /// @return     转换结果
    /// @note       非法文本按 0 处理，由 atoi 语义决定
    int ToInt(const char* text, int fallback)
    {
        if (text == nullptr || *text == '\0')
        {
            return fallback;
        }
        return std::atoi(text);
    }

    /// @brief      打印一个结果集
    /// @param[in]  result 待打印的结果集，可为 nullptr
    /// @warning    内部会调用 mysql_free_result()，调用后不可再使用该结果集
    void PrintResult(MYSQL_RES* result)
    {
        if (result == nullptr)
        {
            std::printf("（无结果集）\n");
            return;
        }

        const unsigned int fields = mysql_num_fields(result);
        MYSQL_ROW row = nullptr;
        unsigned long long rows = 0;

        while ((row = mysql_fetch_row(result)) != nullptr)
        {
            ++rows;
            for (unsigned int i = 0; i < fields; ++i)
            {
                std::printf("%s%s", row[i] != nullptr ? row[i] : "NULL", (i + 1 < fields) ? " | " : "\n");
            }
        }

        mysql_free_result(result);
        std::printf("（共 %llu 行）\n", rows);
    }
} // namespace

int main(int argc, char** argv)
{
    if (argc > 1 && (std::strcmp(argv[1], "-h") == 0 || std::strcmp(argv[1], "--help") == 0))
    {
        PrintUsage(argv[0]);
        return 0;
    }

    const std::string ip = (argc > 1) ? argv[1] : EnvOr("MYSQL_HOST", "127.0.0.1");
    const int port = (argc > 2) ? ToInt(argv[2], 3306) : ToInt(EnvOr("MYSQL_PORT", "3306").c_str(), 3306);
    const std::string user = (argc > 3) ? argv[3] : EnvOr("MYSQL_USER", "root");
    const std::string password = (argc > 4) ? argv[4] : EnvOr("MYSQL_PASSWORD", "");
    const std::string db = (argc > 5) ? argv[5] : EnvOr("MYSQL_DB", "mysql");

    std::printf("连接 %s:%d 用户 %s 数据库 %s\n", ip.c_str(), port, user.c_str(), db.c_str());

    Sql::MySQLPool* pool = Sql::MySQLPool::GetConnectionPool();

    // 初始 2 条、最多 8 条、空闲回收 60s、连接超时 5s、等待队列 100
    if (!pool->init(ip, static_cast<unsigned short>(port), user, password, db, 2, 8, 60, 5, 100))
    {
        std::fprintf(stderr, "连接池初始化失败：请检查地址、账号、密码与数据库名\n");
        return 1;
    }

    Sql::Connection* conn = pool->GetConnection();
    if (conn == nullptr)
    {
        std::fprintf(stderr, "获取连接失败\n");
        pool->shutdown();
        return 1;
    }

    std::printf("\n--- select version(), now() ---\n");
    PrintResult(conn->QueryReturn("select version(), now()"));

    // 用完必须归还
    pool->ReConnection(conn);

    pool->shutdown();
    std::printf("演示结束\n");
    return 0;
}
