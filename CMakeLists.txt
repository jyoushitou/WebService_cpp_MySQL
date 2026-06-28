# CMakeLists.txt
# MySQL RPC 微服务构建配置
cmake_minimum_required(VERSION 3.16)
project(mysql_service VERSION 1.0.0 LANGUAGES CXX)

# ===== 全局设置 =====
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ===== 查找依赖 =====
# MySQL
if(WIN32)
    set(_MYSQL_SEARCH_PATHS
        "$ENV{MYSQL_DIR}"
        "C:/Program Files/MySQL/MySQL Server 8.0"
        "C:/Program Files/MySQL/MySQL Server 5.7"
    )
    
    foreach(_path IN LISTS _MYSQL_SEARCH_PATHS)
        if(EXISTS "${_path}/include/mysql.h")
            set(MYSQL_INCLUDE_DIR "${_path}/include")
            find_library(MYSQL_LIBRARIES
                NAMES libmysql mysqlclient
                PATHS "${_path}/lib"
                NO_DEFAULT_PATH
            )
            break()
        endif()
    endforeach()
else()
    find_path(MYSQL_INCLUDE_DIR NAMES mysql.h PATHS /usr/include/mysql /usr/local/include/mysql)
    find_library(MYSQL_LIBRARIES NAMES mysqlclient PATHS /usr/lib/mysql /usr/lib64/mysql /usr/local/lib/mysql)
endif()

if(NOT MYSQL_INCLUDE_DIR OR NOT MYSQL_LIBRARIES)
    message(FATAL_ERROR "未找到 MySQL C API 开发库，请设置 MYSQL_DIR 环境变量")
endif()

message(STATUS "MySQL 包含目录: ${MYSQL_INCLUDE_DIR}")
message(STATUS "MySQL 库文件:   ${MYSQL_LIBRARIES}")

find_package(Threads REQUIRED)

# Boost（需要 Beast、Asio、JSON）
find_package(Boost REQUIRED COMPONENTS beast json)

# ===== 源文件 =====
set(MYSQL_SOURCES
    source/body/main.cpp
    source/body/mysql.cpp
    source/body/ConnectionPool.cpp
    source/body/Utils.cpp
)

set(MYSQL_HEADERS
    source/include/MyMySQL.h
    source/include/ConnectionPool.h
    source/include/Utils.h
)

# ===== 可执行文件 =====
add_executable(${PROJECT_NAME}
    ${MYSQL_SOURCES}
    ${MYSQL_HEADERS}
)

# ===== 包含目录 =====
target_include_directories(${PROJECT_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/source/include
    ${MYSQL_INCLUDE_DIR}
    ${Boost_INCLUDE_DIRS}
)

# ===== 链接库 =====
target_link_libraries(${PROJECT_NAME} PUBLIC
    ${MYSQL_LIBRARIES}
    ${Boost_LIBRARIES}
    Threads::Threads
)

# ===== 编译选项 =====
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W3 /MP /utf-8 /wd4996)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic -Wno-deprecated-declarations -Wno-unused-parameter)
endif()

# ===== 安装 =====
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)

# ===== 构建信息 =====
message(STATUS "========================================")
message(STATUS "  ${PROJECT_NAME} v${PROJECT_VERSION}")
message(STATUS "========================================")
message(STATUS "  MySQL:       ${MYSQL_INCLUDE_DIR}")
message(STATUS "  Boost:       ${Boost_INCLUDE_DIRS}")
message(STATUS "  C++ 标准:    ${CMAKE_CXX_STANDARD}")
message(STATUS "  构建类型:    ${CMAKE_BUILD_TYPE}")
message(STATUS "========================================")