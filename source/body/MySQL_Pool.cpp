#include "MySQL_Pool.h"

namespace Sql
{
    // 单例外部构造
    MySQLPool* MySQLPool::Get_ConnectionPool()
    {
        static MySQLPool pool;
        return &pool;
    }

    bool MySQLPool::init(const std::string ip, unsigned short port, const std::string user, const std::string password,
                         const std::string db, const int initSize, const int maxSize, const int ConnectSize_init,
                         const int ConnectSize_Max, const int Connect_TimeMax, const int Connect_TimeOut,
                         const int serviceID)
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

        // 服务器ID
        this->serviceID = serviceID;

        // 返回操作码
        return true;
    }

    MySQLPool::MySQLPool()
    {
        if (ip.size() <= 0 || port <= 0 || user.size() <= 0 || password.size() <= 0 || db.size() <= 0 ||
            ConnectSize_init <= 0 || ConnectSize_Max <= 0 || Connect_TimeMax <= 0 || Connect_TimeOut <= 0)
        {
            throw std::invalid_argument("值错误，检查是否初始化，或者数据过期与不全");
        }
        // 创建连接
        for (int i = 0; i < ConnectSize_init; i++)
        {
            // 创建指针
            Connection* conn = new Connection();
            // 连接
            conn->Connect(ip, port, user, password, db);
            // 放入队列
            ConnectionQue.push(conn);
            // 统计数
            ConnectionCnt++;
        }
    }

    // 获取连接
    Connection* MySQLPool::Get_Connection()
    {
        // 如果队列不为空
        if (!ConnectionCnt != 0)
        {
            Connection* conn = ConnectionQue.front();
            ConnectionQue.pop();
            return conn;
        }
        else if (ConnectionCnt == 0 && ConnectionCnt <= ConnectSize_Max)
        {

            Connection* conn = new Connection();
            conn->Connect(ip, port, user, password, db);
            ConnectionCnt++;
        }
    }
} // namespace Sql
