cmake_minimum_required(VERSION 3.20)

# ============================================================
# 自动查找 vcpkg toolchain（必须在 project() 之前设置）
# 可通过以下任一方式覆盖：
#   1. 命令行: cmake -B build -DCMAKE_TOOLCHAIN_FILE=<路径>
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
        message(STATUS "未找到 vcpkg toolchain（可选，不影响本地系统 MySQL 的构建）")
    endif()
endif()

# 本项目依赖用 vcpkg 经典模式安装（utils / libmysql 等），因此显式关闭清单模式：
# 否则若仓库根目录出现 vcpkg.json，vcpkg 会改用 <build>/vcpkg_installed 安装目录，
# 导致已安装的全局包全部“不可见”（甚至被判定为多余而卸载）。
set(VCPKG_MANIFEST_MODE OFF CACHE BOOL "关闭 vcpkg 清单模式，复用全局已安装目录")

project(dbpool
    VERSION 1.0.0
    DESCRIPTION "MySQL connection pool component (header + lib, vcpkg friendly)"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 组件版本（包配置 / pkg-config / 交付目录共用）
set(DBPOOL_VERSION "${PROJECT_VERSION}")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(GNUInstallDirs)

# 源码含中文注释：MSVC 必须按 UTF-8 解析，否则 GBK 尾字节会吞掉换行/注释结尾，
# 导致 C4819 甚至莫名其妙的语法错误。
if(MSVC)
    add_compile_options(/utf-8)
endif()

# ============================================================
# 选项
# ============================================================
option(DBPOOL_BUILD_SHARED "构建动态库（.dll/.so）；OFF 则构建静态库" ON)
option(DBPOOL_BUILD_BUNDLE "生成扁平交付目录（.h 与 .lib/.so 同目录，复制即用）" ON)
option(DBPOOL_BUNDLE_WITH_MYSQL "把 MySQL 客户端头文件/DLL/导入库一并放进交付目录" ON)
option(DBPOOL_BUNDLE_WITH_UTILS "把 Utils 组件的头文件/库一并放进交付目录" ON)
option(DBPOOL_BUILD_TESTS "构建冒烟测试（验证头文件 + 库可正常链接）" OFF)
option(DBPOOL_BUILD_EXAMPLE "构建独立可运行的演示程序 dbpool_demo" ON)
option(DBPOOL_BUILD_MYSQL_SERVICE "同时构建 MySQL RPC 微服务（需要仓库外的 ../../common/cpp）" OFF)

set(DBPOOL_BUNDLE_DIR "${CMAKE_BINARY_DIR}/dbpool-${PROJECT_VERSION}"
    CACHE PATH "扁平交付目录（.h 与 .lib/.so 同目录），默认在构建目录内，不污染源码树")

# ============================================================
# 依赖：MySQL 客户端（系统安装 或 vcpkg 的 libmysql 端口）
#       Utils（vcpkg 独立组件，目标 Utils::Utils）
# 在根目录先查找一次：导入目标在父目录创建后，子目录（component / tests /
# source）都能直接引用；交付目录打包时也复用这些变量。
# ============================================================
find_package(MySQLClient REQUIRED)
find_package(Utils CONFIG REQUIRED)

message(STATUS "Utils 组件: ${Utils_VERSION}（目标 Utils::Utils）")

# ============================================================
# dbpool 组件（含可选的 tests/）
# ============================================================
if(DBPOOL_BUILD_TESTS)
    enable_testing()
endif()
add_subdirectory(component)

# ============================================================
# 独立可运行的演示程序（dbpool_demo.exe）
# ============================================================
if(DBPOOL_BUILD_EXAMPLE)
    add_subdirectory(example)
endif()

# ============================================================
# 可选：MySQL RPC 微服务（依赖仓库外公共组件，默认关闭）
# ============================================================
if(DBPOOL_BUILD_MYSQL_SERVICE)
    add_subdirectory(source)
endif()

# ============================================================
# 摘要
# ============================================================
if(DBPOOL_BUILD_SHARED)
    set(_dbpool_linkage_display "SHARED（.dll + .lib / .so）")
else()
    set(_dbpool_linkage_display "STATIC（.lib / .a）")
endif()

message(STATUS "")
message(STATUS "  dbpool ${PROJECT_VERSION} 构建配置")
message(STATUS "    MySQL 客户端 : ${MySQLClient_INCLUDE_DIR} (${MySQLClient_VERSION})")
message(STATUS "    库类型       : ${_dbpool_linkage_display}")
message(STATUS "    安装前缀     : ${CMAKE_INSTALL_PREFIX}")
message(STATUS "    扁平交付目录 : ${DBPOOL_BUNDLE_DIR}")
message(STATUS "    构建命令     : cmake --build <build-dir>")
message(STATUS "    安装命令     : cmake --install <build-dir> [--prefix <dir>]")
message(STATUS "")
