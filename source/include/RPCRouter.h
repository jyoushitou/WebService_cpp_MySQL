// RPCRouter.h
// RPC 方法路由注册与分发

#ifndef RPC_ROUTER_H
#define RPC_ROUTER_H

#include <string>
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>

#include "RPCProtocol.h"
#include "MyMySQL.h"

// RPC 处理器类型：接收 RPCRequest 和数据库指针，返回 RPCResponse
using RPCHandler = std::function<RPCResponse(const RPCRequest&, MySQL::mysql*)>;

// ===== RPC 方法路由器 =====
class RPCRouter {
public:
    // 注册 RPC 方法
    void Register(const std::string& method, RPCHandler handler) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_handlers[method] = std::move(handler);
    }

    // 分发 RPC 请求到对应处理器
    RPCResponse Handle(const RPCRequest& req, MySQL::mysql* db) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_handlers.find(req.method);
        if (it == m_handlers.end()) {
            return MakeError(req.id, -32601, "方法不存在: " + req.method);
        }
        return it->second(req, db);
    }

private:
    std::unordered_map<std::string, RPCHandler> m_handlers;
    std::mutex m_mutex;

    RPCResponse MakeError(json::value id, int code, const std::string& msg) {
        return {id, true, code, msg, nullptr};
    }
};

#endif // !RPC_ROUTER_H