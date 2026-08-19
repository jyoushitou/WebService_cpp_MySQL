cmake_minimum_required(VERSION 4.3)
# ============================================================
# 自动查找 vcpkg toolchain（必须在 project() 之前设置）
# 可通过以下任一方式覆盖：
#   1. 命令行: cmake .. -DCMAKE_TOOLCHAIN_FILE=<路径>
#   2. 环境变量: VCPKG_ROOT=<vcpkg目录>
# ============================================================
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(_vcpkg_toolchain NOTFOUND)

    # 方法1: 优先使用环境变量 VCPKG_ROOT
    if(DEFINED ENV{VCPKG_ROOT})
        file(TO_CMAKE_PATH "$ENV{VCPKG_ROOT}" _vcpkg_root)
        set(_vcpkg_toolchain "${_vcpkg_root}/scripts/buildsystems/vcpkg.cmake")
        if(NOT EXISTS "${_vcpkg_toolchain}")
            set(_vcpkg_toolchain NOTFOUND)
        endif()
    endif()

    # 方法2: 尝试常见安装路径
    if(NOT _vcpkg_toolchain)
        foreach(_path
            "C:/vcpkg"
            "D:/vcpkg"
            "D:/app/vcpkg"
            "E:/app/vcpkg"
            "$ENV{USERPROFILE}/vcpkg")
            set(_candidate "${_path}/scripts/buildsystems/vcpkg.cmake")
            if(EXISTS "${_candidate}")
                set(_vcpkg_toolchain "${_candidate}")
                break()
            endif()
        endforeach()
    endif()

    if(_vcpkg_toolchain)
        set(CMAKE_TOOLCHAIN_FILE "${_vcpkg_toolchain}" CACHE FILEPATH "vcpkg toolchain file")
        message(STATUS "找到 vcpkg toolchain: ${_vcpkg_toolchain}")
    else()
        message(WARNING "未找到 vcpkg toolchain。\n"
            "如果 Protobuf 查找失败，请手动指定：\n"
            "  cmake .. -DCMAKE_TOOLCHAIN_FILE=<vcpkg目录>/scripts/buildsystems/vcpkg.cmake\n"
            "或设置环境变量 VCPKG_ROOT=<vcpkg目录>")
    endif()
endif()

project(MySQL VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Protobuf REQUIRED)
find_package(Boost COMPONENTS json asio REQUIRED)

# ============================================================
# 查找 MySQL（提供 mysql.h 与 libmysql.lib）
# 默认路径: "C:/Program Files/MySQL/MySQL Server 8.0"
# 可通过 -DMYSQL_ROOT="D:/path/to/mysql" 覆盖
# ============================================================
set(MYSQL_ROOT "C:/Program Files/MySQL/MySQL Server 8.0" CACHE PATH "MySQL 安装目录（包含 include/ 和 lib/）")
find_path(MYSQL_INCLUDE_DIR NAMES mysql.h PATHS "${MYSQL_ROOT}/include" NO_DEFAULT_PATH)
find_library(MYSQL_LIBRARY NAMES libmysql PATHS "${MYSQL_ROOT}/lib" NO_DEFAULT_PATH)
if(NOT MYSQL_INCLUDE_DIR OR NOT MYSQL_LIBRARY)
    message(FATAL_ERROR
        "未找到 MySQL 开发库。请确认 MySQL 已安装，或指定：\n"
        "  cmake .. -DMYSQL_ROOT=\"C:/Program Files/MySQL/MySQL Server 8.0\"\n"
        "当前 MYSQL_ROOT = \"${MYSQL_ROOT}\"")
endif()

add_executable(${PROJECT_NAME}
    main.cpp
    ../../common/cpp/body/Common.pb.cc
    ../../common/cpp/body/MySQL.pb.cc
    ../../common/cpp/body/Utils.cpp
    ../../common/cpp/body/ThreadPool.cpp
    ../../common/cpp/body/NetConnection.cpp
    ../../common/cpp/body/NetServer.cpp
    body/work.cpp
    body/MySQL_Pool.cpp
    body/MySQL_Connection.cpp
)

# Common.pb.cc 中 include 的是 "common/Common.pb.h"，需要 ../common
# main.cpp 中 include 的是 "Common.pb.h"，需要 ../common/include
# ============================================================
# 解决 Common.pb.cc 中 #include "common/Common.pb.h" 的路径映射
# 在构建目录下创建 include/common，存放生成的头文件副本
# ============================================================
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/include/common")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/../../common/cpp/include/Common.pb.h"
    DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/include/common/")

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common/cpp/include
    ${CMAKE_CURRENT_BINARY_DIR}/include
    ${MYSQL_INCLUDE_DIR}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    protobuf::libprotobuf
    Boost::json
    ${MYSQL_LIBRARY}
)

# 避免 winsock.h 与 winsock2.h 冲突，以及 min/max 宏干扰
if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
endif()

# 让 MSVC 按 UTF-8 解析源码，解决 C4819 警告（中文注释/字符串）
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /utf-8)
endif()

# ============================================================
# 部署 MySQL 运行时 DLL（libmysql.dll）
# 优先从 MySQL 的 lib/ 目录查找，其次尝试 bin/ 目录
# 使用 copy_if_different 避免每次构建重复复制
# ============================================================
find_file(MYSQL_RUNTIME_DLL
    NAMES libmysql.dll
    PATHS "${MYSQL_ROOT}/lib" "${MYSQL_ROOT}/bin"
    NO_DEFAULT_PATH
)
if(MYSQL_RUNTIME_DLL)
    message(STATUS "找到 MySQL 运行时 DLL: ${MYSQL_RUNTIME_DLL}")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${MYSQL_RUNTIME_DLL}"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
        COMMENT "复制 libmysql.dll 到输出目录"
    )
else()
    message(WARNING
        "未找到 libmysql.dll（已查找 \"${MYSQL_ROOT}/lib\" 和 \"${MYSQL_ROOT}/bin\"），\n"
        "程序运行时若缺少该 DLL 将无法启动，请确认 MySQL 安装完整或手动部署。")
endif()
