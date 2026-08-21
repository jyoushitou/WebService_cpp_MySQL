#include "SQLWork.h"

int main()
{
    Utils::init();

    int port = 60908;

    RunServer(port);

    Utils::Out::Out_Msg("MySQL微服务已退出");

    return 0;
}