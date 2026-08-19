// MySQL Work
#pragma once

// 官方库
#include <iostream>
#include <string>
#include <memory>
#include <thread>
#include <csignal>
#include <atomic>

// 工具类
#include "Utils.h"

// 消息端
#include "NetServer.h"
#include "Message.h"
#include <boost/asio.hpp>
#include "Common.pb.h"
#include "MySQL.pb.h"

// MySQL专属
#include "MySQL_Connection.h"
#include "MySQL_Pool.h"
#include "ThreadPool.h"

#ifdef _WIN32
// 需要显式包含，否则 BOOL / WINAPI / DWORD 未定义
#include <windows.h>
#endif

//===变量===

// 通讯

// 配置中心服务ID，后续替换为真正的校验
constexpr int ConfigServiceID = 4;

// 全局服务器指针，供信号处理函数使用
inline std::shared_ptr<Net::Server::Server> server_ptr;

//===全局指针===

// 全局MySQL连接池指针
inline Sql::MySQLPool* pool = nullptr;

// 线程池指针
inline std::unique_ptr<ThreadPool::ThreadPool> threadpool = nullptr;

// 配置全局变量（TODO后期从配置中心获取）

// 服务器ID
inline int serviceID = 2;
// ip
inline std::string ip = "";
// 端口
inline uint32_t sql_port = 0;
// 用户
inline std::string user = "";
// 密码
inline std::string password = "";
// 目标的数据库
inline std::string db = "";
// 初始的连接量
inline uint32_t InitSize = 0;
// 最大的连接量
inline uint32_t MaxSize = 0;
// 最大时间
inline uint32_t TimeMax = 0;
// 断连时间
inline uint32_t TimeOut = 0;

// 线程池线程数
inline uint32_t ThreadPoolSize = 0;

// MySQL初始化判断
inline std::atomic<bool> InitMySQL(false);

// 函数

// 通讯

// 服务器启动函数
void RunServer(int port = 60908);

// 校验配置中心传来的参数
bool CheckConfigMessage(const sql::sql_node& node);

//===MySQL===

// 启动MySQL
void StartMySQL();

// MySQL工作
void MySQLWork(Sql::MySQLTask task);

// 停止工作
void ShutDownMySQL();

// 线程池和连接池初始化
void Start();