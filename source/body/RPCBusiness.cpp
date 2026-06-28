// RPCBusiness.cpp
// RPC 业务方法实现

#include "RPCBusiness.h"
#include "RPCProtocol.h"
#include <iostream>

// ===== 业务方法实现 =====
void RegisterBusinessMethods(RPCRouter& router, MySQL::mysql* db) {
    // 用户登录鉴权
    router.Register("user.login", [](const RPCRequest& req, MySQL::mysql* db) -> RPCResponse {
        try {
            auto& params = req.params.as_object();
            std::string name = params.at("username").as_string().c_str();
            std::string password = params.at("password").as_string().c_str();
            
            std::string avatar;
            int permission = db->User(name, password, &avatar);
            
            json::object result;
            result["permission"] = permission;
            result["avatar"] = avatar;
            
            if (permission > 0) {
                result["message"] = "登录成功";
            } else if (permission == 0) {
                result["message"] = "用户名或密码错误";
            } else {
                result["message"] = "数据库错误";
            }
            
            return JSONRPCProcessor::MakeResult(req.id, result);
        } catch (const std::exception& e) {
            return JSONRPCProcessor::MakeError(req.id, -32602, "参数错误: " + std::string(e.what()));
        }
    });

    // 获取用户 ID
    router.Register("user.getUserId", [](const RPCRequest& req, MySQL::mysql* db) -> RPCResponse {
        try {
            auto& params = req.params.as_object();
            std::string name = params.at("username").as_string().c_str();
            
            int userId = db->Get_UserId(name);
            
            json::object result;
            result["user_id"] = userId;
            return JSONRPCProcessor::MakeResult(req.id, result);
        } catch (const std::exception& e) {
            return JSONRPCProcessor::MakeError(req.id, -32602, "参数错误: " + std::string(e.what()));
        }
    });

    // 获取用户头像
    router.Register("user.getAvatar", [](const RPCRequest& req, MySQL::mysql* db) -> RPCResponse {
        try {
            auto& params = req.params.as_object();
            std::string name = params.at("username").as_string().c_str();
            
            std::string avatar = db->Get_Avatar(name);
            
            json::object result;
            result["avatar"] = avatar;
            return JSONRPCProcessor::MakeResult(req.id, result);
        } catch (const std::exception& e) {
            return JSONRPCProcessor::MakeError(req.id, -32602, "参数错误: " + std::string(e.what()));
        }
    });

    // 创建数据库
    router.Register("database.create", [](const RPCRequest& req, MySQL::mysql* db) -> RPCResponse {
        try {
            auto& params = req.params.as_object();
            std::string dbName = params.at("name").as_string().c_str();
            
            bool success = db->Create_DataBases(dbName);
            
            json::object result;
            result["success"] = !success;
            result["message"] = success ? "创建失败" : "数据库已就绪";
            return JSONRPCProcessor::MakeResult(req.id, result);
        } catch (const std::exception& e) {
            return JSONRPCProcessor::MakeError(req.id, -32602, "参数错误: " + std::string(e.what()));
        }
    });

    // 健康检查
    router.Register("system.ping", [](const RPCRequest& req, MySQL::mysql* db) -> RPCResponse {
        json::object result;
        result["status"] = "ok";
        result["service"] = "mysql_service";
        result["version"] = "1.0.0";
        return JSONRPCProcessor::MakeResult(req.id, result);
    });
}