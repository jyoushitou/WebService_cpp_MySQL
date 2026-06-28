// WebSocketSession.cpp
// WebSocket 会话处理实现

#include "WebSocketSession.h"
#include "RPCProtocol.h"
#include <iostream>
#include <atomic>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;

extern std::atomic<bool> g_running;

// ===== WebSocket 会话处理 =====
void HandleSession(tcp::socket socket, RPCRouter& router, MySQL::mysql* db) {
    try {
        websocket::stream<tcp::socket> ws(std::move(socket));
        ws.accept();

        beast::flat_buffer buffer;
        
        while (g_running) {
            buffer.clear();
            ws.read(buffer);
            
            std::string requestStr = beast::buffers_to_string(buffer.data());
            std::cout << "[RPC] 收到请求: " << requestStr.substr(0, 100) << "..." << std::endl;
            
            try {
                auto req = JSONRPCProcessor::Parse(requestStr);
                auto res = router.Handle(req, db);
                std::string responseStr = JSONRPCProcessor::Serialize(res);
                
                ws.write(net::buffer(responseStr));
            } catch (const std::exception& e) {
                auto errRes = JSONRPCProcessor::MakeError(nullptr, -32700, "解析错误: " + std::string(e.what()));
                std::string errStr = JSONRPCProcessor::Serialize(errRes);
                ws.write(net::buffer(errStr));
            }
        }
    } catch (const beast::system_error& se) {
        if (se.code() != websocket::error::closed) {
            std::cerr << "[RPC] 会话错误: " << se.what() << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "[RPC] 会话异常: " << e.what() << std::endl;
    }
}