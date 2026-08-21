#include "MySQL_Connection.h"

namespace Sql
{
    // 构造初始化连接
    Connection::Connection(bool TempConnect)
    {

        Utils::Out::Out_Msg("开始初始化连接");

        // 设置连接类型
        this->TempConnect = TempConnect;

        // 初始化状态
        Connecting = false;

        // 初始化conn指针
        conn = nullptr;

        // 刷新使用时间
        SetUpdateLastTime();

        Utils::Out::Out_Msg("初始化连接完成");
    }

    // 关闭连接并析构
    Connection::~Connection()
    {
        Utils::Out::Out_Msg("开始析构");
        try
        {
            // 直接关闭
            DisConnect();
        }
        catch (...)
        {
            Utils::Out::Out_Err("有错误，析构失败");
        }
    }

    // 连接MySQL
    bool Connection::Connect(const std::string ip, const unsigned int port, const std::string user,
                             const std::string password, const std::string db, const unsigned int TimeOut)
    {
        Utils::Out::Out_Msg("检查是否已有连接");

        // 判断是否连接
        if (Connecting)
        {
            Utils::Out::Out_Msg("已有连接，关闭连接后再创建新连接");

            // 关闭连接
            DisConnect();
        }

        Utils::Out::Out_Msg("开始连接");

        // 初始化句柄
        conn = mysql_init(nullptr);

        if (!conn)
        {
            Utils::Out::Out_Err("MySQL连接失败请重连");
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
            Utils::Out::Out_Err("遇到错误：" + std::string(mysql_error(conn)));
            // 关闭连接
            mysql_close(conn);
            // 指针置空
            conn = nullptr;

            Utils::Out::Out_Msg("取消连接");

            return false;
        }

        Utils::Out::Out_Msg("连接到MySQL");

        // 设置字符串返回值
        mysql_set_character_set(conn, "utf8mb4");

        // 更新连接状态
        Connecting = true;

        // 更新连接状态
        SetUpdateLastTime();

        return true;
    }

    // 关闭连接
    bool Connection::DisConnect()
    {
        Utils::Out::Out_Msg("检查是否有连接");

        if (conn)
        {
            Utils::Out::Out_Msg("有连接，关闭连接");
            // 关闭MySQL连接
            mysql_close(conn);
            // 重置conn
            conn = nullptr;
            Utils::Out::Out_Msg("关闭连接成功");
        }
        else
        {
            Utils::Out::Out_Msg("没有连接");
        }
        // 更新状态
        Connecting = false;

        return true;
    }

    // 自定义语句封装
    // 无返回的自定义语句函数
    bool Connection::QueryNoReturn(const std::string sql)
    {
        Utils::Out::Out_Msg("校验是否有连接");
        // 判断是否连接
        if (!Connecting || !conn)
        {
            Utils::Out::Out_Err("MySQL数据库未连接，请先连接");
            return false;
        }

        Utils::Out::Out_Msg("开始进行操作");

        // sql传入数据库
        if (mysql_query(conn, sql.c_str()) != 0)
        {
            Utils::Out::Out_Err(mysql_error(conn));
            return false;
        }

        Utils::Out::Out_Msg("操作完成");

        // 更新最后编辑时间
        SetUpdateLastTime();

        return true;
    }

    // 自定义语句封装
    // 有返回的自定义语句函数
    MYSQL_RES* Connection::QueryReturn(const std::string sql)
    {
        Utils::Out::Out_Msg("校验是否有连接");

        // 判断是否连接
        if (!Connecting || !conn)
        {
            Utils::Out::Out_Err("MySQL数据库未连接");
            return nullptr;
        }

        // sql传入数据库
        if (mysql_query(conn, sql.c_str()) != 0)
        {
            Utils::Out::Out_Err(mysql_error(conn));
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
                Utils::Out::Out_Err(mysql_error(conn));
                return nullptr;
            }

            Utils::Out::Out_Err("数据库无结果");

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
            Utils::Out::Out_Err(mysql_error(conn));

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

    // 查询

    // 全部查询
    MYSQL_RES* Connection::Select(const std::string& list, const std::string& tables)
    {
        std::string sql = "select " + list + " from " + tables + ";";
        return QueryReturn(sql);
    }
    // 条件查询（一个条件）
    MYSQL_RES* Connection::SelectWhere(const std::string& list, const std::string& tables, const std::string& where)
    {
        std::string sql = "select " + list + " from " + tables + " where " + where + ";";
        return QueryReturn(sql);
    }
    // 模糊条件查询（一个条件）
    MYSQL_RES* Connection::SelectLike(const std::string& list, const std::string& tables, const std::string& where,
                                      const std::string& value)
    {
        std::string sql = "select " + list + " from " + tables + " where " + where + " Like " + value + ";";
        return QueryReturn(sql);
    }

    // 插入
    bool Connection::Insert(const std::string& tables, const std::string& list, const std::string& value)
    {
        std::string sql = "insert into " + tables + " (" + list + ") values (" + value + ");";
        return QueryNoReturn(sql);
    }

    bool Connection::Update(const std::string& table, const std::string& list, const std::string& where)
    {
        std::string sql = "update " + table + " set " + list + " where " + where;
        return QueryNoReturn(sql);
    }

    bool Connection::Delete(const std::string& table)
    {
        std::string sql = "delete from " + table;
        return QueryNoReturn(sql);
    }

    bool Connection::DeleteWhere(const std::string& table, const std::string& where)
    {
        std::string sql = "delete from " + table + " where " + where;
        return QueryNoReturn(sql);
    }
} // namespace Sql