#include "MySQL_Pool.h"

namespace Sql
{
    // 单例外部构造
    MySQLPool* MySQLPool::GetConnectionPool()
    {
        static MySQLPool pool;
        return &pool;
    }

    bool MySQLPool::init(const std::string& ip, unsigned short port, const std::string& user,
                         const std::string& password, const std::string& db, int initSize, int maxSize,
                         int ConnectSize_init, int ConnectSize_Max, int Connect_TimeMax, int Connect_TimeOut)
    {
        // 赋MySQL服务器地址
        this->ip = ip;
        // 赋MySQL服务器端口
        this->port = port;
        // 赋MySQL用户
        this->user = user;
        // 赋MySQL用户密码
        this->password = password;
        // 赋MySQL要连接的数据库
        this->db = db;

        // 赋初始连接数
        this->ConnectSize_init = ConnectSize_init;
        // 赋最大连接数
        this->ConnectSize_Max = ConnectSize_Max;
        // 赋最大空闲连接时间
        this->Connect_TimeMax = Connect_TimeMax;
        // 赋超时时间
        this->Connect_TimeOut = Connect_TimeOut;

        // 返回操作码
        return true;
    }
} // namespace Sql
