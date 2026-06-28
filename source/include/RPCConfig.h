// RPCConfig.h
// RPC 服务配置结构体与命令行解析

#ifndef RPC_CONFIG_H
#define RPC_CONFIG_H

#include <string>
#include <iostream>

// ===== RPC 服务配置 =====
struct RPCConfig {
    std::string host = "0.0.0.0";
    int port = 60907;           // MySQL RPC 服务端口
    int threadCount = 4;

    // 解析命令行参数
    bool ParseArgs(int argc, char* argv[]) {
        for (int i = 1; i < argc; i++) {
            std::string arg = argv[i];
            if (arg == "--port" && i + 1 < argc) {
                port = std::stoi(argv[++i]);
            } else if (arg == "--host" && i + 1 < argc) {
                host = argv[++i];
            } else if (arg == "--threads" && i + 1 < argc) {
                threadCount = std::stoi(argv[++i]);
            } else if (arg == "--help") {
                PrintHelp();
                return false;
            }
        }
        return true;
    }

    void PrintHelp() const {
        std::cout << "用法: mysql_service [选项]\n"
                  << "  --port <port>       RPC 服务端口 (默认: 60907)\n"
                  << "  --host <host>       绑定地址 (默认: 0.0.0.0)\n"
                  << "  --threads <count>   I/O 线程数 (默认: 4)\n"
                  << "  --help              显示此帮助\n";
    }
};

#endif // !RPC_CONFIG_H