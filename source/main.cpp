#include "SQLWork.h"

int main()
{
    Utils::init();

    // 默认端口
    int port = 60908;
    if (!(std::cin >> port))
    // 输入失败（管道/EOF 等）时使用默认值
    {
        Utils::Out_Msg("输入无效，使用默认端口 60908", serviceID);
        std::cin.clear();
    }

    RunServer(port, serviceID);

    Utils::Out_Msg("服务器退出", serviceID);
}