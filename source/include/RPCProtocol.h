// RPCProtocol.h
// JSON-RPC 2.0 协议请求/响应类型定义与序列化

#ifndef RPC_PROTOCOL_H
#define RPC_PROTOCOL_H

#include <string>
#include <stdexcept>
#include <boost/json.hpp>

namespace json = boost::json;

// ===== RPC 请求结构 =====
struct RPCRequest {
    std::string jsonrpc;
    std::string method;
    json::value params;
    json::value id;
};

// ===== RPC 响应结构 =====
struct RPCResponse {
    json::value id;
    bool isError = false;
    int errorCode = 0;
    std::string errorMessage;
    json::value result;
};

// ===== JSON-RPC 2.0 协议处理器 =====
class JSONRPCProcessor {
public:
    // 解析 JSON 字符串为 RPCRequest
    static RPCRequest Parse(const std::string& jsonStr) {
        RPCRequest req;
        try {
            auto jv = json::parse(jsonStr);
            auto& obj = jv.as_object();
            
            req.jsonrpc = obj.at("jsonrpc").as_string().c_str();
            req.method = obj.at("method").as_string().c_str();
            req.id = obj.contains("id") ? obj.at("id") : nullptr;
            
            if (obj.contains("params")) {
                req.params = obj.at("params");
            }
        } catch (const std::exception& e) {
            throw std::runtime_error("JSON 解析失败: " + std::string(e.what()));
        }
        return req;
    }

    // 序列化 RPCResponse 为 JSON 字符串
    static std::string Serialize(const RPCResponse& res) {
        json::object obj;
        
        obj["jsonrpc"] = "2.0";
        obj["id"] = res.id;
        
        if (res.isError) {
            json::object error;
            error["code"] = res.errorCode;
            error["message"] = res.errorMessage;
            obj["error"] = error;
        } else {
            obj["result"] = res.result;
        }
        
        return json::serialize(obj);
    }

    // 构造成功响应
    static RPCResponse MakeResult(json::value id, json::value result) {
        return {id, false, 0, "", std::move(result)};
    }

    // 构造错误响应
    static RPCResponse MakeError(json::value id, int code, const std::string& msg) {
        return {id, true, code, msg, nullptr};
    }
};

#endif // !RPC_PROTOCOL_H