#include "SQLWork.h"

// 配置全局服务器ID
int Utils::serviceID = 2;

bool CheckConfigMessage(const sql::sql_node& node)
{
    // 来源是否配置中心
    // TODO后期使用token校验
    if (node.head().serviceid() != ConfigServiceID)
    {
        Utils::Out::Out_Err("校验失败：来源服务ID不对");
        return false;
    }

    // 字段完整性校验
    sql::sql_init init;
    if (!init.ParseFromString(node.msg()))
    {
        Utils::Out::Out_Err("校验失败：配置解析不了");
        return false;
    }

    if (init.ip().empty() || init.port() == 0 || init.db().empty())
    {
        Utils::Out::Out_Err("校验失败：ip/port/db 不完整");
        return false;
    }

    return true;
}

// 服务器启动函数
void RunServer(int port)
{
    Utils::Out::Out_Msg("正在启动通讯端口");

    // 注册停止回调（把本服务的清理逻辑统一注册到 Utils）
    Utils::Exit::RegisterStopCallback(
        []()
        {
            if (server_ptr)
                server_ptr->Stop();
            ShutDownMySQL();
        });

    // 2注册统一信号处理（替代本地的 OnSignal / ConsoleCtrlHandler）
    std::signal(SIGINT, Utils::Exit::Onsignal);
#ifdef _WIN32
    SetConsoleCtrlHandler(Utils::Exit::ConsoleCtrlHandler, TRUE);
#endif

    // 创建上下文
    boost::asio::io_context io;

    // 创建监听端点
    boost::asio::ip::tcp::endpoint ep(boost::asio::ip::tcp::v4(), port);

    // 创建服务器对象
    server_ptr = std::make_shared<Net::Server::Server>(io, ep);

    // 开始接收连接
    server_ptr->StartAccept();

    Utils::Out::Out_Msg("服务器启动，监听端口 " + std::to_string(port) + " ...等待连接中");

    // 单独一个线程运行 io_context
    std::thread io_thread([&io]() { io.run(); });

    // 主线程循环：等待消息并处理，然后回复客户端
    while (Utils::Exit::running.load())
    {
        auto [session, msg_id, msg] = server_ptr->WaitForMessage();

        // 收到终止信号
        if (!session && msg == "close")
        {
            Utils::Out::Out_Msg("服务器正在退出...");

            // 触发统一结束函数
            Utils::Exit::RecviceExit();

            break;
        }

        sql::sql_node node;
        if (!node.ParseFromString(msg))
        {
            Utils::Out::Out_Err("解析失败，跳过该消息");
            session->Reply(msg_id, "解析错误，请重新发送消息！");
            continue;
        }

        if (node.type() == 0)
        {
            Utils::Out::Out_Msg("收到握手信息！");
            session->Reply(msg_id, "服务器收到握手信息！");
        }

        // 处理传来的数据类型
        if (node.type() == 1)
        {
            Utils::Out::Out_Msg("收到配置中心注册/更新，正在判断是否合法");
            if (!CheckConfigMessage(node))
            {
                Utils::Out::Out_Err("配置校验损坏不通过，已拒绝");
                session->Reply(msg_id, "不要传这些坏的配置>_<喵！");
                continue;
            }

            Utils::Out::Out_Msg("校验通过!正在应用");

            // 解析初始化的
            sql::sql_init init;
            if (!init.ParseFromString(node.msg()))
            {
                Utils::Out::Out_Err("配置消息解析失败，忽略");
                session->Reply(msg_id, "请重新发送");
                continue;
            }

            ShutDownMySQL();

            // 为全局变量赋值
            ip = init.ip();
            sql_port = init.port();
            user = init.user();
            password = init.password();
            db = init.db();
            InitSize = init.initsize();
            MaxSize = init.maxsize();
            TimeMax = init.timemax();
            TimeOut = init.timeout();
            ThreadPoolSize = init.threadpoolsize();

            Start();
            session->Reply(msg_id, "配置已更新喵！");
            continue;
        }
        // TODO 完成对应的解析
        if (node.type() > 1)
        {
            Utils::Out::Out_Msg("收到业务请求，type=" + std::to_string(node.type()) + " 消息为：" + msg);

            // 限流检查
            constexpr size_t MaxWorkQueue = 500;
            if (!threadpool || !pool || threadpool->QueueSize() > MaxWorkQueue ||
                pool->WaitQueueSize() >= Wait_Queue_Max)
            { // TODO
                session->Reply(msg_id, "服务器繁忙，请稍后重试");
                continue;
            }

            // 在 MySQLWork 内部打包
            MySQLWork(session, msg_id, node.type(), node.msg());

            continue;
        }
    }

    // 停止服务器（幂等，可安全重复调用）
    server_ptr->Stop();

    // 等待接收处理完
    io_thread.join();

    // 资源清理
    ShutDownMySQL();
}

// 启动MySQL
void StartMySQL()
{
    Utils::Out::Out_Msg("正在创建MySQL连接池");

    // 创建连接池实例
    pool = Sql::MySQLPool::GetConnectionPool();

    if (!pool->init(ip, sql_port, user, password, db, InitSize, MaxSize, TimeMax, TimeOut, Wait_Queue_Max))
    {
        Utils::Out::Out_Err("MySQL连接池初始化失败");
        return;
    }

    InitMySQL = true;

    Utils::Out::Out_Msg("创建MySQL连接池成功");
}

// 将 MYSQL_RES* 格式化为回复文本（并释放）
std::string recv_select(int type, MYSQL_RES* res)
{
    if (!res)
    {
        Utils::Out::Out_Msg("未查询数据");
        return "查询失败";
    }

    Utils::Out::Out_Msg("查询到数据!开始解析并构建回复");

    // 构建回复字符串
    webuser::WebUser Select_user;

    unsigned int fields = mysql_num_fields(res);
    MYSQL_ROW row;

    switch (type)
    {
    case 1:
        // 取出行
        row = mysql_fetch_row(res);
        if (!row) // 查无此人
        {
            mysql_free_result(res);
            return "查询成功，无数据";
        }
        // 构建查询结果
        Select_user.set_uid((int)row[0]);
        Select_user.set_name(row[1]);
        Select_user.set_password(row[2]);
        Select_user.set_permissions((int)row[3]);

        break;

    default:
        break;
    }

    // while ((row = mysql_fetch_row(res)))
    // {
    //     for (unsigned int i = 0; i < fields; i++)
    //     {
    //         if (i > 0)
    //             reply += "\t";
    //         if (row[i])
    //             reply += row[i];
    //     }
    //     reply += "\n";
    // }

    // mysql_free_result(res);

    // 释放mysql字符集
    mysql_free_result(res);

    // 转换成字符串
    std::string reply = Select_user.SerializeAsString();

    Utils::Out::Out_Msg("构建完成！");

    return reply.empty() ? "查询成功，无数据" : reply;
}

// 构建队列
Sql::MySQLTask BuildMySQLTask(const int& type, const std::string& msg)
{
    Utils::Out::Out_Msg("开始构建任务队列");

    switch (type)
    {
    // 当type为1时查询
    case 1:
    {
        Utils::Out::Out_Msg("为查询请求，开始转化成对应查询");

        // 查看查询类型
        sql::select select;
        if (!select.ParseFromString(msg))
        {
            return nullptr;
        }

        switch (select.type())
        {

        // 当为查询用户
        case 1:
        {
            // 转化为查询用户
            sql::CheckUser CheckUser;
            if (!CheckUser.ParseFromString(select.msg()))
            {
                return nullptr;
            }

            return [select, CheckUser](Sql::Connection* conn) -> std::string
            {
                // 获取句柄
                MYSQL* raw = conn->GetConn();
                // 创建缓冲区
                size_t len = CheckUser.name().size() * 2 + 1;
                char* escaped = new char[len];

                // 使用缓冲区创造缓冲大小
                mysql_real_escape_string(raw, escaped, CheckUser.name().c_str(), CheckUser.name().size());

                // 构造sql语句
                std::string where = "name = '" + std::string(escaped) + "'";

                // 删除缓冲区
                delete[] escaped;

                return recv_select(select.type(), conn->Select(CheckUser.head().table(), where));
            };
        }
        default:
            return nullptr;
        }
    }
    default:
        return nullptr;
    }
}

// 将MySQL打包成任务
void MySQLWork(std::shared_ptr<Net::Server::Session> session, uint32_t msg_id, int node_type,
               const std::string& request_data)
{
    Utils::Out::Out_Msg("正在将请求打包成任务");

    // 根据 type 构造业务 task
    Sql::MySQLTask task = BuildMySQLTask(node_type, request_data);

    Utils::Out::Out_Msg("校验打包任务");

    // 未知类型 或 参数不完整
    if (!task)
    {
        if (session)
            session->Reply(msg_id, "未知操作类型或参数缺失");
        return;
    }

    Utils::Out::Out_Msg("准备获取连接");

    // 非阻塞尝试拿连接
    Sql::Connection* conn = pool->GetConnection();

    if (conn)
    {
        Utils::Out::Out_Msg("获取到连接，放入到线程池队列等待执行");
        // 有连接：打包完整任务给线程池
        threadpool->enques([task, conn, session, msg_id]() { RunTaskAndReply(task, conn, session, msg_id); });
    }
    else
    {
        Utils::Out::Out_Msg("当前连接池耗尽，正在等待连接");
        // 无连接：注册等待回调
        bool ok = pool->WaitForConnection(
            [task, session, msg_id](Sql::Connection* conn)
            {
                Utils::Out::Out_Msg("查看线程池是否初始化");
                if (threadpool)
                {
                    threadpool->enques([task, conn, session, msg_id]()
                                       { RunTaskAndReply(task, conn, session, msg_id); });
                }
                else if (pool)
                {
                    Utils::Out::Out_Err("未初始化线程池，归还连接");
                    pool->ReConnection(conn);
                }
            });

        Utils::Out::Out_Msg("超过服务器的最大流量，拒绝请求");
        if (!ok)
        {
            // 限流拒绝：等待队列满了
            if (session)
                session->Reply(msg_id, "服务器繁忙，请稍后重试");
        }
    }
}

// 停止工作
void ShutDownMySQL()
{
    // 防止重复进入
    static std::mutex mtx;
    std::lock_guard<std::mutex> lock(mtx);

    if (threadpool)
    {
        // 析构会 join 等待所有任务完成
        threadpool.reset();
        threadpool = nullptr;
    }

    // 重置pool
    if (InitMySQL.exchange(false))
    {
        if (pool)
        {
            pool->shutdown();
        }
    }

    InitMySQL = false;

    Utils::Out::Out_Msg("MySQL服务已关闭");
}

// 线程池和连接池初始化
void Start()
{
    Utils::Out::Out_Msg("MySQL连接池是否已经初始化");
    // 检查是否初始化
    if (InitMySQL)
    {
        Utils::Out::Out_Msg("已经初始化MySQL连接池！");
        return;
    }

    StartMySQL();

    if (!InitMySQL)
    {
        return;
    }

    Utils::Out::Out_Msg("正在检查是否创建了线程池");

    // 创建连接池
    if (!threadpool)
    {
        threadpool = std::make_unique<ThreadPool::ThreadPool>(ThreadPoolSize);
    }
}

// 执行任务并回复、归还连接（线程池工作线程中执行）
void RunTaskAndReply(Sql::MySQLTask task, Sql::Connection* conn, std::shared_ptr<Net::Server::Session> session,
                     uint32_t msg_id)
{
    Utils::Out::Out_Msg("检查连接池是否创建");

    if (!conn || !pool)
    {
        if (session)
            session->Reply(msg_id, "服务器内部错误");
        return;
    }

    std::string reply;

    try
    {
        reply = task(conn); // 执行特化函数
    }
    catch (const std::exception& e)
    {
        reply = std::string("执行失败：") + e.what();
    }
    catch (...)
    {
        reply = "执行失败：未知异常";
    }

    Utils::Out::Out_Msg("构建完成，先归还连接");

    // 归还连接
    pool->ReConnection(conn);

    // 回复
    if (session)
        session->Reply(msg_id, reply);
}