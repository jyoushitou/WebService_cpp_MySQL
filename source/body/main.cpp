// main.cpp
// MySQL RPC 微服务入口
// 通过 WebSocket JSON-RPC 对外提供数据库访问
// 使用连接池管理数据库连接，支持高并发

#include "RPCConfig.h"
#include "RPCRouter.h"
#include "RPCBusiness.h"
#include "WebSocketSession.h"
#include "MyMySQL.h"
#include "ConnectionPool.h"

#include <iostream>
#include <string>
#include <functional>
#include <memory>
#include <atomic>
#include <vector>
#include <thread>
#include <chrono>
#include <csignal>

#include <boost/beast/core.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>

namespace net = boost::asio;
using tcp = net::ip::tcp;

// ===== 全局变量 =====
static RPCConfig g_config;
std::atomic<bool> g_running{true};

// ===== 信号处理 =====
void SignalHandler(int signal) {
    std::cout << "\n收到信号 " << signal << "，正在关闭服务..." << std::endl;
    g_running = false;
}

// ===== 主函数 =====
int main(int argc, char* argv[]) {
    // 解析命令行参数
    if (!g_config.ParseArgs(argc, argv)) {
        return 0;
    }

    // 注册信号处理
    signal(SIGINT, SignalHandler);
    signal(SIGTERM, SignalHandler);

    std::cout << "========================================" << std::endl;
    std::cout << "  MySQL RPC 微服务 v1.0.0 (连接池版)" << std::endl;
    std::cout << "========================================" << std::endl;

    // 初始化连接池（在创建 mysql 对象之前）
    MySQL::PoolConfig poolConfig;
    poolConfig.minSize = 2;           // 最小连接数
    poolConfig.maxSize = 10;          // 最大连接数
    poolConfig.timeoutSeconds = 30;   // 获取连接超时
    poolConfig.idleTimeoutSeconds = 300; // 空闲连接超时
    MySQL::mysql::InitPool(poolConfig);
    std::cout << "[Main] 连接池初始化完成" << std::endl;

    // 初始化数据库（使用连接池）
    MySQL::mysql db;
    std::cout << "[Main] 数据库服务就绪" << std::endl;

    // 确保默认数据库存在
    db.Create_DataBases();

    // 注册 RPC 方法
    RPCRouter router;
    RegisterBusinessMethods(router, &db);
    std::cout << "[Main] RPC 方法注册完成" << std::endl;

    // 启动 RPC 服务
    try {
        net::io_context ioc(g_config.threadCount);
        tcp::acceptor acceptor(ioc, tcp::endpoint(
            net::ip::make_address(g_config.host), g_config.port
        ));

        std::cout << "[Main] RPC 服务已启动: ws://" << g_config.host 
                  << ":" << g_config.port << std::endl;
        std::cout << "[Main] 按 Ctrl+C 停止服务" << std::endl;

        // 接受连接
        std::function<void()> doAccept;
        doAccept = [&]() {
            acceptor.async_accept([&](beast::error_code ec, tcp::socket socket) {
                if (!ec) {
                    std::thread(&HandleSession, std::move(socket), std::ref(router), &db).detach();
                }
                if (g_running) {
                    doAccept();
                }
            });
        };
        doAccept();

        // 启动 I/O 线程
        std::vector<std::thread> threads;
        for (int i = 0; i < g_config.threadCount; i++) {
            threads.emplace_back([&ioc]() {
                ioc.run();
            });
        }

        // 定期健康检查（每5分钟）
        std::thread healthThread([&]() {
            while (g_running) {
                std::this_thread::sleep_for(std::chrono::minutes(5));
                if (g_running) {
                    MySQL::mysql::PoolHealthCheck();
                }
            }
        });

        // 等待停止信号
        while (g_running) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        // 优雅关闭
        std::cout << "[Main] 正在关闭服务..." << std::endl;
        acceptor.close();
        ioc.stop();

        for (auto& t : threads) {
            if (t.joinable()) t.join();
        }
        if (healthThread.joinable()) healthThread.join();

    } catch (const std::exception& e) {
        std::cerr << "[Main] 启动失败: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "[Main] 服务已停止" << std::endl;
    return 0;
}