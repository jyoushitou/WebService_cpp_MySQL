#pragma once

#include <iostream>
#include <string>

#include "MySQL_Connection.h"
#include "MySQL_Pool.h"
#include "ThreadPool.h"

// 启动MySQL
void StartMySQL(const std::string ip, unsigned short port, const std::string user, const std::string password,
                const std::string db, const int initSize, const int maxSize, const int connectsize_init,
                const int connectsize_max, const int connect_timemax, const int connect_timeout, const int serviceID);