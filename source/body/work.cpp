#include "work.h"

// 退出逻辑
void GracefulShutdown()
{
    bool expected = false;
    if (g_stop_called.compare_exchange_strong(expected, true))
    {
        Utils::Out_Msg("收到退出信号，正在停止服务器...", serviceID);
        g_exit_flag = true;
        if (g_server)
        {
            g_server->Stop();
        }
    }
}

// Ctrl+C / SIGTERM 处理函数
void OnSignal(int)
{
    GracefulShutdown();
}

#ifdef _WIN32
// Windows 控制台关闭事件处理（taskkill、关闭窗口等）
BOOL WINAPI ConsoleCtrlHandler(DWORD ctrlType)
{
    switch (ctrlType)
    {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
    case CTRL_CLOSE_EVENT:
    case CTRL_LOGOFF_EVENT:
    case CTRL_SHUTDOWN_EVENT:
        GracefulShutdown();
        return TRUE;
    default:
        return FALSE;
    }
}
#endif

bool CheckConfigMessage(const sql::sql_node& node)
{
    // 来源是否配置中心
    // TODO后期使用token校验
    if (node.head().serviceid() != ConfigServiceID)
    {
        Utils::Out_Err("校验失败：来源服务ID不对", serviceID);
        return false;
    }

    // 字段完整性校验
    sql::sql_init init;
    if (!init.ParseFromString(node.msg()))
    {
        Utils::Out_Err("校验失败：配置解析不了", serviceID);
        return false;
    }

    if (init.ip().empty() || init.port() == 0 || init.db().empty())
    {
        Utils::Out_Err("校验失败：ip/port/db 不完整", serviceID);
        return false;
    }

    return true;
}

// 服务器启动函数
void RunServer(int port, int serviceID)
{
    Utils::Out_Msg("正在启动通讯端口", serviceID);

    // 创建上下文
    boost::asio::io_context io;

    // 创建监听端点
    boost::asio::ip::tcp::endpoint ep(boost::asio::ip::tcp::v4(), port);

    // 创建服务器对象
    g_server = std::make_shared<Net::Server::Server>(io, ep, serviceID);

    // 开始接收连接
    g_server->StartAccept();

    Utils::Out_Msg("服务器启动，监听端口 " + std::to_string(port) + " ...等待连接中", serviceID);

    // 注册 Ctrl+C 处理
    std::signal(SIGINT, OnSignal);

#ifdef _WIN32
    // 注册 Windows 控制台事件处理（taskkill / 关闭窗口等也能优雅退出）
    SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);
#endif

    // 单独一个线程运行 io_context
    std::thread io_thread([&io]() { io.run(); });

    // 主线程循环：等待消息并处理，然后回复客户端
    while (!g_exit_flag.load())
    {
        auto [session, msg_id, msg] = g_server->WaitForMessage();

        // 收到终止信号
        if (!session && msg == "close")
        {
            Utils::Out_Msg("服务器正在退出...", serviceID);
            break;
        }

        sql::sql_node node;
        if (!node.ParseFromString(msg))
        {
            Utils::Out_Err("解析失败，跳过该消息", serviceID);
            session->Reply(msg_id, "解析错误，请重新发送消息！");
            continue;
        }

        // 构建回复文字
        std::string replaymsg = "";

        // 处理传来的数据类型
        if (node.type() == 1)
        {
            Utils::Out_Msg("收到配置中心注册/更新，正在判断是否合法", serviceID);
            if (!CheckConfigMessage(node))
            {
                Utils::Out_Err("配置校验损坏不通过，已拒绝", serviceID);
                session->Reply(msg_id, "不要传这些坏的配置>_<喵！");
                continue;
            }

            Utils::Out_Msg("校验通过!正在应用", serviceID);

            // 解析初始化的
            sql::sql_init init;
            if (!init.ParseFromString(node.msg()))
            {
                Utils::Out_Err("配置消息解析失败，忽略", serviceID);
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
        }
    }

    // 停止服务器（幂等，可安全重复调用）
    g_server->Stop();

    // 等待接收处理完
    io_thread.join();

    // 资源清理
    ShutDownMySQL();

    Utils::Out_Msg("服务器已退出", serviceID);
}

// 启动MySQL
void StartMySQL()
{
    Utils::Out_Msg("正在创建MySQL连接池", serviceID);

    // 创建连接池实例
    pool = Sql::MySQLPool::GetConnectionPool();

    if (!pool->init(ip, sql_port, user, password, db, InitSize, MaxSize, TimeMax, TimeOut, serviceID))
    {
        Utils::Out_Err("MySQL连接池初始化失败", serviceID);
        ShutDownMySQL();
        return;
    }

    InitMySQL = true;
}

// 工作MySQL
void MySQLWork(Sql::MySQLTask task)
{
    // 判断工作任务和连接池是否正确
    if (!pool || !task || !threadpool)
    {
        Utils::Out_Err("线程池未初始化或传入任务为空", serviceID);
        return;
    }
    // 放入线程池中
    threadpool->enques(
        [task]()
        {
            Utils::Out_Msg("正在执行", serviceID);

            // 从连接池获取连接
            Sql::Connection* conn = pool->GetConnection();

            // 判断是否合法
            if (!conn)
            {
                Utils::Out_Err("获取MySQL连接失败（超时或连接池已满）", serviceID);
                return;
            }

            try
            {
                task(conn);
            }
            catch (...)
            {
                Utils::Out_Err("未知异常", serviceID);
            }

            // 归还连接
            pool->ReConnection(conn);
        });
}

// 停止工作
void ShutDownMySQL()
{
    if (threadpool)
    {
        // 析构会 join 等待所有任务完成
        threadpool.reset();
        threadpool = nullptr;
    }

    if (pool)
    {
        pool->shutdown();
        pool = nullptr;
    }

    InitMySQL = false;

    Utils::Out_Msg("MySQL服务已关闭", serviceID);
}

// 线程池和连接池初始化
void Start()
{
    Utils::Out_Msg("MySQL连接池是否已经初始化", serviceID);
    // 检查是否初始化
    if (InitMySQL)
    {
        Utils::Out_Msg("已经初始化MySQL连接池！", serviceID);
        return;
    }

    StartMySQL();

    if (!InitMySQL)
    {
        return;
    }

    Utils::Out_Msg("正在检查是否创建了线程池", serviceID);

    // 创建连接池
    if (!threadpool)
    {
        threadpool = std::make_unique<ThreadPool::ThreadPool>(ThreadPoolSize);
    }
}