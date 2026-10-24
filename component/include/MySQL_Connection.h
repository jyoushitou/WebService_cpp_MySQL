/// @file        MySQL_Connection.h
/// @brief       单条 MySQL 连接：建连、Ping、增删改查封装
/// @author      jyoushitou
/// @date        2026-10-24
/// @copyright   Copyright (c) 2026

// 防止重复包含
#pragma once

#ifdef _WIN32
// mysql.h 与 windows.h 都要用 socket 相关定义，统一走 winsock2
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#endif

#include <string>
#include <chrono>
#include <exception>

#include <mysql.h>

// 统一日志 / IO 组件（vcpkg 包 utils，导入目标 Utils::Utils）
#include "Utils.h"

namespace Sql
{
    /// @class       Connection
    /// @brief       单条 MySQL 连接
    /// @details     封装 MySQL C API：建连、断连、Ping 与常用增删改查
    /// @note        由 MySQLPool 创建与保管，业务方通过连接池借出使用
    /// @warning     非线程安全，同一条连接同一时刻只能由一个线程使用
    class Connection
    {
    public:
        /// @brief      构造函数
        /// @details    初始化状态与最后使用时间，此时尚未建立连接
        /// @param[in]  TempConnect true 临时连接（空闲可被回收）；false 常驻连接
        /// @note       构造不发起网络连接，需再调用 Connect()
        Connection(bool TempConnect);

        /// @brief      析构函数
        /// @details    关闭已建立的连接，异常在内部吞掉
        ~Connection();

        /// @brief      连接 MySQL
        /// @details    已连接时先断开；随后配置超时与自动重连并建立连接
        /// @param[in]  ip       数据库地址
        /// @param[in]  port     数据库端口
        /// @param[in]  user     数据库账号
        /// @param[in]  password 数据库密码
        /// @param[in]  db       数据库名
        /// @param[in]  TimeOut  连接超时（秒）
        /// @return     true 连接成功；false 失败（内部已关闭句柄）
        /// @warning    失败时不会抛出异常，错误原因写入日志
        bool Connect(const std::string ip, const unsigned int port, const std::string user, const std::string password,
                     const std::string db, const unsigned int TimeOut);

        /// @brief      检查连接是否可用
        /// @details    向 MySQL 发送 ping；失败时把连接状态置为未连接
        /// @return     true 连接正常；false 未连接或 ping 失败
        bool Ping();

        /// @brief      获取连接类型
        /// @return     true 临时连接；false 常驻连接
        bool GetTempConnect();

        /// @brief      获取原始句柄
        /// @return     MYSQL* 句柄，未连接时为 nullptr
        /// @warning    调用方不得自行 mysql_close()，所有权仍属于本对象
        MYSQL* GetConn();

        /// @brief      获取距最后一次使用的秒数
        /// @return     空闲秒数
        /// @note       监控线程据此判断连接是否超时
        long long GetLastSeconds();

        /// @brief      刷新最后使用时间
        /// @details    每次成功执行语句或归还连接时调用
        void SetUpdateLastTime();

        // === 查询 ===

        /// @brief      全表查询
        /// @param[in]  row    列名，如 "id,name"
        /// @param[in]  tables 表名
        /// @return     结果集；失败返回 nullptr
        /// @warning    结果集需调用方 mysql_free_result() 释放
        MYSQL_RES* Select(const std::string& row, const std::string& tables);

        /// @brief      带一个条件的查询
        /// @param[in]  row    列名
        /// @param[in]  tables 表名
        /// @param[in]  where  条件表达式，如 "id=1"
        /// @return     结果集；失败返回 nullptr
        MYSQL_RES* SelectWhere(const std::string& row, const std::string& tables, const std::string& where);

        /// @brief      模糊查询
        /// @param[in]  row    列名
        /// @param[in]  tables 表名
        /// @param[in]  where  条件列
        /// @param[in]  value  模糊匹配值，如 "'%abc%'"
        /// @return     结果集；失败返回 nullptr
        /// @warning    拼接 SQL 不做转义，外部输入请先自行转义
        MYSQL_RES* SelectLike(const std::string& row, const std::string& tables, const std::string& where,
                              const std::string& value);

        // === 插入 ===

        /// @brief      插入一行
        /// @param[in]  tables 表名
        /// @param[in]  list   列名，如 "name,age"
        /// @param[in]  value  值，如 "'tom',18"
        /// @return     true 执行成功；false 失败
        bool Insert(const std::string& tables, const std::string& list, const std::string& value);

        /// @brief      日志归档（未实现）
        /// @return     目前固定返回 false
        /// @warning    TODO：待补充实现
        bool InsertLog();

        // === 更新 ===

        /// @brief      更新数据
        /// @param[in]  table 表名
        /// @param[in]  list  赋值表达式，如 "age=18"
        /// @param[in]  where 条件表达式
        /// @return     true 执行成功；false 失败
        bool Update(const std::string& table, const std::string& list, const std::string& where);

        // === 删除 ===

        /// @brief      清空表
        /// @param[in]  table 表名
        /// @return     true 执行成功；false 失败
        /// @warning    删除全表数据，不可恢复
        bool Delete(const std::string& table);

        /// @brief      按条件删除
        /// @param[in]  tables 表名
        /// @param[in]  where  条件表达式
        /// @return     true 执行成功；false 失败
        bool DeleteWhere(const std::string& tables, const std::string& where);

        // === 自定义语句 ===

        /// @brief      执行无返回结果的语句
        /// @param[in]  sql 完整 SQL
        /// @return     true 执行成功；false 未连接或执行失败
        bool QueryNoReturn(const std::string sql);

        /// @brief      执行有返回结果的语句
        /// @param[in]  sql 完整 SQL
        /// @return     结果集；未连接、执行失败或无结果时返回 nullptr
        /// @warning    结果集需调用方 mysql_free_result() 释放
        MYSQL_RES* QueryReturn(const std::string sql);

    private:
        /// @brief      禁止拷贝构造
        Connection(const Connection&) = delete;

        /// @brief      禁止赋值
        Connection& operator=(const Connection&) = delete;

        /// @brief      断开连接
        /// @details    关闭句柄并清空状态，可重复调用
        /// @return     固定返回 true
        bool DisConnect();

        MYSQL* conn;                                             ///< 底层 MySQL 句柄
        bool Connecting;                                         ///< 是否已连接
        std::chrono::steady_clock::time_point LastUsedTime;      ///< 最后一次使用时间
        bool TempConnect;                                        ///< 连接类型：true 临时连接
    };
} // namespace Sql
