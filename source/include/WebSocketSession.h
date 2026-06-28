// WebSocketSession.h
// WebSocket 会话处理声明

#ifndef WEBSOCKET_SESSION_H
#define WEBSOCKET_SESSION_H

#include <boost/asio/ip/tcp.hpp>
#include "RPCRouter.h"
#include "MyMySQL.h"

// 处理单个 WebSocket 会话
void HandleSession(boost::asio::ip::tcp::socket socket, RPCRouter& router, MySQL::mysql* db);

#endif // !WEBSOCKET_SESSION_H