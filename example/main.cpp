/// @file        main.cpp
/// @brief       dbpool 独立演示程序：初始化连接池 -> 查询 -> 归还连接 -> 关闭
/// @details     可直接运行，用于验证“头文件 + 库”确实可用。
///              用法：dbpool_demo [ip] [port] [user] [password] [db]
///              未给出的参数可用环境变量 MYSQL_HOST / MYSQL_PORT / MYSQL_USER /
///              MYSQL_PASSWORD / MYSQL_DB 提供，都有默认值。

#include "MySQL_Pool.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace
{
    void PrintUsage(const char* exe)
    {
        std::printf(
            "用法: %s [ip] [port] [user] [password] [db]\n"
            "示例: %s 127.0.0.1 3306 root 123456 test\n"
            "环境变量: MYSQL_HOST / MYSQL_PORT / MYSQL_USER / MYSQL_PASSWORD / MYSQL_DB\n",
            exe, exe);
    }

    std::string EnvOr(const char* key, const char* fallback)
    {
        const char* value = std::getenv(key);
        return (value != nullptr && *value != '\0') ? std::string(value) : std::string(fallback);
    }

    int ToInt(const char* text, int fallback)
    {
        if (text == nullptr || *text == '\0')
        {
            return fallback;
        }
        return std::atoi(text);
    }

    // 打印一个结果集
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
