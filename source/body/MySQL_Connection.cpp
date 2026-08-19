#include "MySQL_Connection.h"

namespace Sql
{
    // 构造初始化连接
    Connection::Connection(int serviceID, bool TempConnect)
    {
        // 初始化服务器ID
        this->serviceID = serviceID;

        // 设置连接类型
        this->TempConnect = TempConnect;

        // 初始化状态
        Connecting = false;

        // 初始化conn指针
        conn = nullptr;

        // 刷新使用时间
        SetUpdateLastTime();
    }

    // 关闭连接并析构
    Connection::~Connection()
    {
        try
        {
            // 直接关闭
            DisConnect();
        }
        catch (...)
        {
            Utils::Out_Err("有错误，析构失败", serviceID);
        }
    }

    // 连接MySQL
    bool Connection::Connect(const std::string ip, const unsigned int port, const std::string user,
                             const std::string password, const std::string db, const unsigned int TimeOut)
    {
        // 判断是否连接
        if (Connecting)
        {
            // 关闭连接
            DisConnect();
        }

        // 初始化句柄
        conn = mysql_init(nullptr);

        if (!conn)
        {
            Utils::Out_Err("MySQL连接失败请重连", serviceID);
            return false;
        }

        // 设置连接超时时间
        mysql_options(conn, MYSQL_OPT_CONNECT_TIMEOUT, &TimeOut);

        // 设置自动重连
        bool reconnect = true;
        mysql_options(conn, MYSQL_OPT_RECONNECT, &reconnect);

        // 建立连接
        if (!mysql_real_connect(conn, ip.c_str(), user.c_str(), password.c_str(), db.c_str(), port, nullptr, 0))
        {
            // 连接失败的处理

            Utils::Out_Err(mysql_error(conn), serviceID);

            // 关闭连接
            mysql_close(conn);
            // 指针置空
            conn = nullptr;

            return false;
        }

        // 设置字符串返回值
        mysql_set_character_set(conn, "utf8m4");

        // 更新连接状态
        Connecting = true;

        // 更新连接状态
        SetUpdateLastTime();

        return true;
    }

    // 关闭连接
    bool Connection::DisConnect()
    {
        if (conn)
        {
            // 关闭MySQL连接
            mysql_close(conn);
            // 重置conn
            conn = nullptr;
        }
        // 更新状态
        Connecting = false;

        return true;
    }

    // 自定义语句封装
    // 无返回的自定义语句函数
    bool Connection::Query_NoReturn(const std::string sql)
    {
        // 判断是否连接
        if (!Connecting || !conn)
        {
            Utils::Out_Err("MySQL数据库未连接", serviceID);
            return false;
        }

        // sql传入数据库
        if (mysql_query(conn, sql.c_str()) != 0)
        {
            Utils::Out_Err(mysql_error(conn), serviceID);
            return false;
        }

        // 更新最后编辑时间
        SetUpdateLastTime();

        return true;
    }

    // 自定义语句封装
    // 有返回的自定义语句函数
    MYSQL_RES* Connection::Query_Return(const std::string sql)
    {
        // 判断是否连接
        if (!Connecting || !conn)
        {
            Utils::Out_Err("MySQL数据库未连接", serviceID);
            return nullptr;
        }

        // sql传入数据库
        if (mysql_query(conn, sql.c_str()) != 0)
        {
            Utils::Out_Err(mysql_error(conn), serviceID);
            return nullptr;
        }

        // 获取返回的结果指针
        MYSQL_RES* res = mysql_store_result(conn);

        // 判断是否有
        if (!res)
        {
            // res无但是filed有
            if (mysql_field_count(conn) > 0)
            {
                Utils::Out_Err(mysql_error(conn), serviceID);
                return nullptr;
            }

            Utils::Out_Err("数据库无结果", serviceID);

            return nullptr;
        }

        // 更新最后编辑时间
        SetUpdateLastTime();

        return res;
    }

    // 连接状态
    bool Connection::Ping()
    {
        if (!conn || !Connecting)
        {
            // 本来就无连接
            return false;
        }

        // 发送ping指令给MySQL
        if (mysql_ping(conn) != 0)
        {
            Utils::Out_Err(mysql_error(conn), serviceID);

            // 更新连接状态
            Connecting = false;

            return false;
        }

        return true;
    }

    // 获取原始句柄
    MYSQL* Connection::GetConn()
    {
        return conn;
    }

    // 获取最后的更新时间
    long long Connection::GetLastSeconds()
    {
        return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - LastUsedTime)
            .count();
    }

    // 获取连接类型
    bool Connection::GetTempConnect()
    {
        return TempConnect;
    }

    // 设置更新的时间
    void Connection::SetUpdateLastTime()
    {
        LastUsedTime = std::chrono::steady_clock::now();
    }
} // namespace Sql