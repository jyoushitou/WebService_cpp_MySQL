# ============================================================================
# source/CMakeLists.txt —— MySQL RPC 微服务（可选目标）
#
# 该可执行文件依赖仓库之外的公共组件（../../Common_CPP 或 ../../common/cpp：
# NetServer / ThreadPool / Message / protobuf 生成代码）。公共组件缺失、或源码
# 尚未迁移到新版 Utils 接口时，本目录默认不参与构建，只构建 dbpool 组件即可。
#
# 打开方式：cmake -B build -DDBPOOL_BUILD_MYSQL_SERVICE=ON
# ============================================================================
cmake_minimum_required(VERSION 3.20)

# 单独在本目录配置时（cd source && cmake -B build）充当顶层工程
if(NOT CMAKE_PROJECT_NAME)
    if(NOT DEFINED CMAKE_TOOLCHAIN_FILE AND DEFINED ENV{VCPKG_ROOT})
        file(TO_CMAKE_PATH "$ENV{VCPKG_ROOT}" _vcpkg_root)
        if(EXISTS "${_vcpkg_root}/scripts/buildsystems/vcpkg.cmake")
            set(CMAKE_TOOLCHAIN_FILE "${_vcpkg_root}/scripts/buildsystems/vcpkg.cmake"
                CACHE FILEPATH "vcpkg toolchain file")
        endif()
    endif()

    project(SQL VERSION 1.0.0 LANGUAGES CXX)

    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake")

# ---------------------------------------------------------------------------
# 前置检查：仓库外的公共组件
#   · 旧布局：WebServer/common/cpp
#   · 新布局：WebServer/Common_CPP（Net / Utils 已拆成独立仓库）
# ---------------------------------------------------------------------------
set(_common_dir "")
foreach(_candidate
    "${CMAKE_CURRENT_SOURCE_DIR}/../../Common_CPP"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../common/cpp")
    if(EXISTS "${_candidate}/include/Utils.h")
        set(_common_dir "${_candidate}")
        break()
    endif()
endforeach()

if(NOT _common_dir)
    message(FATAL_ERROR
        "找不到公共组件目录（NetServer / ThreadPool / Message / protobuf 生成代码）。\n"
        "已尝试：\n"
        "  ${CMAKE_CURRENT_SOURCE_DIR}/../../Common_CPP\n"
        "  ${CMAKE_CURRENT_SOURCE_DIR}/../../common/cpp\n"
        "只构建 dbpool 组件时请使用默认配置（-DDBPOOL_BUILD_MYSQL_SERVICE=OFF）。")
endif()

# 微服务源码仍在使用旧版公共组件接口（Utils::Out::Out_Msg / Utils::Exit::RegisterStopCallback
# / Utils::serviceID 等），而 vcpkg 里的 Utils 已改为 outMsg / registerStopCallback。
# 这里显式拦住，避免输出一堆含义不明的编译错误。
option(DBPOOL_ALLOW_LEGACY_SERVICE
    "允许构建尚未迁移到新版 Utils 接口的微服务" OFF)
if(NOT DBPOOL_ALLOW_LEGACY_SERVICE)
    message(FATAL_ERROR
        "微服务源码尚未迁移到新版 Utils 接口（Utils::Out::outMsg 等）。\n"
        "迁移完成前请保持 -DDBPOOL_BUILD_MYSQL_SERVICE=OFF（默认）。\n"
        "确认要继续尝试可加 -DDBPOOL_ALLOW_LEGACY_SERVICE=ON。")
endif()

find_package(Protobuf REQUIRED)
find_package(Boost COMPONENTS json asio REQUIRED)
find_package(Utils CONFIG REQUIRED)

# ---------------------------------------------------------------------------
# dbpool 组件（作为子目录引入时已存在）
# ---------------------------------------------------------------------------
if(NOT TARGET dbpool)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../component"
        "${CMAKE_CURRENT_BINARY_DIR}/dbpool_component")
endif()

# 让本目录也能拿到 MySQL 客户端信息（单独配置时组件在本目录作用域之外）
find_package(MySQLClient REQUIRED)

# ---------------------------------------------------------------------------
# 服务可执行文件
# ---------------------------------------------------------------------------
add_executable(${PROJECT_NAME}
    main.cpp
    "${_common_dir}/body/Common.pb.cc"
    "${_common_dir}/body/MySQL.pb.cc"
    "${_common_dir}/body/User.pb.cc"
    "${_common_dir}/body/Utils.cpp"
    "${_common_dir}/body/ThreadPool.cpp"
    "${_common_dir}/body/NetConnection.cpp"
    "${_common_dir}/body/NetServer.cpp"
    body/SQLWork.cpp)

# Common.pb.cc 中 include 的是 "common/Common.pb.h"，需要把生成头文件放到
# include/common 下（保持与旧构建一致）
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/include/common")
if(EXISTS "${_common_dir}/include/Common.pb.h")
    file(COPY "${_common_dir}/include/Common.pb.h"
        DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/include/common/")
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${_common_dir}/include"
    "${CMAKE_CURRENT_BINARY_DIR}/include")

target_link_libraries(${PROJECT_NAME} PRIVATE
    dbpool
    Utils::Utils
    protobuf::libprotobuf
    Boost::json
    Boost::asio)

# 避免 winsock.h 与 winsock2.h 冲突，以及 min/max 宏干扰
if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
endif()

# MSVC 按 UTF-8 解析源码，解决 C4819（中文注释/字符串）
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /utf-8)
endif()

# ---------------------------------------------------------------------------
# 部署 MySQL 运行时 DLL（libmysql.dll）
# ---------------------------------------------------------------------------
if(MySQLClient_RUNTIME_DLL)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${MySQLClient_RUNTIME_DLL}" "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
        COMMENT "复制 libmysql.dll 到输出目录"
        VERBATIM)
else()
    message(STATUS "未找到 libmysql.dll，运行时请手动部署（详见 README）")
endif()
