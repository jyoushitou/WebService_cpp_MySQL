/// @file        MySQL_Connection.cpp
/// @brief       单条 MySQL 连接实现：建连、Ping、增删改查封装
/// @author      jyoushitou
/// @date        2026-10-24
/// @copyright   Copyright (c) 2026

#include "MySQL_Connection.h"

namespace Sql
{
    /// @brief      构造函数
    /// @param[in]  TempConnect true 临时连接；false 常驻连接
    /// @note       构造不发起网络连接，需再调用 Connect()
    Connection::Connection(bool TempConnect)
    {

        Utils::Out::outMsg("开始初始化连接");

        // 设置连接类型
        this->TempConnect = TempConnect;

        // 初始化状态
        Connecting = false;

        // 初始化conn指针
        conn = nullptr;

        // 刷新使用时间
        SetUpdateLastTime();

        Utils::Out::outMsg("初始化连接完成");
    }

    /// @brief      析构函数
    /// @details    关闭已建立的连接，异常在内部吞掉
    Connection::~Connection()
    {
        Utils::Out::outMsg("开始析构");
        try
        {
            // 直接关闭
            DisConnect();
        }
        catch (...)
        {
            Utils::Out::outErr("有错误，析构失败");
        }
    }

    /// @brief      连接 MySQL
    /// @details    已连接时先断开；随后配置超时与自动重连并建立连接
    /// @param[in]  ip       数据库地址
    /// @param[in]  port     数据库端口
    /// @param[in]  user     数据库账号
    /// @param[in]  password 数据库密码
    /// @param[in]  db       数据库名
    /// @param[in]  TimeOut  连接超时（秒）
    /// @return     true 连接成功；false 失败（内部已关闭句柄）
    bool Connection::Connect(const std::string ip, const unsigned int port, const std::string user,
                             const std::string password, const std::string db, const unsigned int TimeOut)
    {
        Utils::Out::outMsg("检查是否已有连接");

        // 判断是否连接
        if (Connecting)
        {
            Utils::Out::outMsg("已有连接，关闭连接后再创建新连接");

            // 关闭连接
            DisConnect();
        }

        Utils::Out::outMsg("开始连接");

        // 初始化句柄
        conn = mysql_init(nullptr);

        if (!conn)
        {
            Utils::Out::outErr("MySQL连接失败请重连");
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
            Utils::Out::outErr("遇到错误：" + std::string(mysql_error(conn)));
            // 关闭连接
            mysql_close(conn);
            // 指针置空
            conn = nullptr;

            Utils::Out::outMsg("取消连接");

            return false;
        }

        Utils::Out::outMsg("连接到MySQL");

        // 设置字符串返回值
        mysql_set_character_set(conn, "utf8mb4");

        // 更新连接状态
        Connecting = true;

        // 更新连接状态
        SetUpdateLastTime();

        return true;
    }

    /// @brief      断开连接
    /// @details    关闭句柄并清空状态，可重复调用
    /// @return     固定返回 true
    bool Connection::DisConnect()
    {
        Utils::Out::outMsg("检查是否有连接");

        if (conn)
        {
            Utils::Out::outMsg("有连接，关闭连接");
            // 关闭MySQL连接
            mysql_close(conn);
            // 重置conn
            conn = nullptr;
            Utils::Out::outMsg("关闭连接成功");
        }
        else
        {
            Utils::Out::outMsg("没有连接");
        }
        // 更新状态
        Connecting = false;

        return true;
    }

    /// @brief      执行无返回结果的语句
    /// @param[in]  sql 完整 SQL
    /// @return     true 执行成功；false 未连接或执行失败
    bool Connection::QueryNoReturn(const std::string sql)
    {
        Utils::Out::outMsg("校验是否有连接");
        // 判断是否连接
        if (!Connecting || !conn)
        {
            Utils::Out::outErr("MySQL数据库未连接，请先连接");
            return false;
        }

        Utils::Out::outMsg("开始进行操作");

        // sql传入数据库
        if (mysql_query(conn, sql.c_str()) != 0)
        {
            Utils::Out::outErr(mysql_error(conn));
            return false;
        }

        Utils::Out::outMsg("操作完成");

        // 更新最后编辑时间
        SetUpdateLastTime();

        return true;
    }

    /// @brief      执行有返回结果的语句
    /// @param[in]  sql 完整 SQL
    /// @return     结果集；未连接、执行失败或无结果时返回 nullptr
    /// @warning    结果集需调用方 mysql_free_result() 释放
    MYSQL_RES* Connection::QueryReturn(const std::string sql)
    {
        Utils::Out::outMsg("校验是否有连接");

        // 判断是否连接
        if (!Connecting || !conn)
        {
            Utils::Out::outErr("MySQL数据库未连接");
            return nullptr;
        }

        // sql传入数据库
        if (mysql_query(conn, sql.c_str()) != 0)
        {
            Utils::Out::outErr(mysql_error(conn));
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
                Utils::Out::outErr(mysql_error(conn));
                return nullptr;
            }

            Utils::Out::outErr("数据库无结果");

            return nullptr;
        }

        // 更新最后编辑时间
        SetUpdateLastTime();

        return res;
    }

    /// @brief      检查连接是否可用
    /// @details    向 MySQL 发送 ping；失败时把连接状态置为未连接
    /// @return     true 连接正常；false 未连接或 ping 失败
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
            Utils::Out::outErr(mysql_error(conn));

            // 更新连接状态
            Connecting = false;

            return false;
        }

        return true;
    }

    /// @brief      获取原始句柄
    /// @return     MYSQL* 句柄，未连接时为 nullptr
    /// @warning    调用方不得自行 mysql_close()
    MYSQL* Connection::GetConn()
    {
        return conn;
    }

    /// @brief      获取距最后一次使用的秒数
    /// @return     空闲秒数
    long long Connection::GetLastSeconds()
    {
        return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - LastUsedTime)
            .count();
    }

    /// @brief      获取连接类型
    /// @return     true 临时连接；false 常驻连接
    bool Connection::GetTempConnect()
    {
        return TempConnect;
    }

    /// @brief      刷新最后使用时间
    /// @details    每次成功执行语句或归还连接时调用
    void Connection::SetUpdateLastTime()
    {
        LastUsedTime = std::chrono::steady_clock::now();
    }

    /// @brief      全表查询
    /// @param[in]  list   列名，如 "id,name"
    /// @param[in]  tables 表名
    /// @return     结果集；失败返回 nullptr
    MYSQL_RES* Connection::Select(const std::string& list, const std::string& tables)
    {
        std::string sql = "select " + list + " from " + tables + ";";
        return QueryReturn(sql);
    }
    /// @brief      带一个条件的查询
    /// @param[in]  list   列名
    /// @param[in]  tables 表名
    /// @param[in]  where  条件表达式，如 "id=1"
    /// @return     结果集；失败返回 nullptr
    MYSQL_RES* Connection::SelectWhere(const std::string& list, const std::string& tables, const std::string& where)
    {
        std::string sql = "select " + list + " from " + tables + " where " + where + ";";
        return QueryReturn(sql);
    }
    /// @brief      模糊查询
    /// @param[in]  list   列名
    /// @param[in]  tables 表名
    /// @param[in]  where  条件列
    /// @param[in]  value  匹配值，如 "'%abc%'"
    /// @return     结果集；失败返回 nullptr
    /// @warning    不做转义，外部输入请先自行转义
    MYSQL_RES* Connection::SelectLike(const std::string& list, const std::string& tables, const std::string& where,
                                      const std::string& value)
    {
        std::string sql = "select " + list + " from " + tables + " where " + where + " Like " + value + ";";
        return QueryReturn(sql);
    }

    /// @brief      插入一行
    /// @param[in]  tables 表名
    /// @param[in]  list   列名，如 "name,age"
    /// @param[in]  value  值，如 "'tom',18"
    /// @return     true 执行成功；false 失败
    bool Connection::Insert(const std::string& tables, const std::string& list, const std::string& value)
    {
        std::string sql = "insert into " + tables + " (" + list + ") values (" + value + ");";
        return QueryNoReturn(sql);
    }

    /// @brief      更新数据
    /// @param[in]  table 表名
    /// @param[in]  list  赋值表达式，如 "age=18"
    /// @param[in]  where 条件表达式
    /// @return     true 执行成功；false 失败
    bool Connection::Update(const std::string& table, const std::string& list, const std::string& where)
    {
        std::string sql = "update " + table + " set " + list + " where " + where;
        return QueryNoReturn(sql);
    }

    /// @brief      清空表
    /// @param[in]  table 表名
    /// @return     true 执行成功；false 失败
    /// @warning    删除全表数据，不可恢复
    bool Connection::Delete(const std::string& table)
    {
        std::string sql = "delete from " + table;
        return QueryNoReturn(sql);
    }

    /// @brief      按条件删除
    /// @param[in]  table 表名
    /// @param[in]  where 条件表达式
    /// @return     true 执行成功；false 失败
    bool Connection::DeleteWhere(const std::string& table, const std::string& where)
    {
        std::string sql = "delete from " + table + " where " + where;
        return QueryNoReturn(sql);
    }
} // namespace Sql