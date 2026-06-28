// RPCBusiness.h
// RPC 业务方法声明

#ifndef RPC_BUSINESS_H
#define RPC_BUSINESS_H

#include "RPCRouter.h"
#include "MyMySQL.h"

// 注册所有业务 RPC 方法到路由器
void RegisterBusinessMethods(RPCRouter& router, MySQL::mysql* db);

#endif // !RPC_BUSINESS_H