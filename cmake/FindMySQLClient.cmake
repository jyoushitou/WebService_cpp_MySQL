# ============================================================================
# FindMySQLClient.cmake —— 查找 MySQL 客户端开发库
#
# 支持三种来源（按优先级）：
#   1. 显式指定 MYSQL_ROOT（cmake -DMYSQL_ROOT="C:/Program Files/MySQL/MySQL Server 8.0"）
#   2. vcpkg 的 libmysql 端口（find_package(unofficial-libmysql CONFIG)）
#   3. 系统安装的 MySQL / MariaDB 客户端（自动探测常见路径）
#
# 成功后会提供：
#   MySQL::Client               导入接口目标，直接 target_link_libraries 即可
#   MySQLClient_INCLUDE_DIR     含 mysql.h 的目录（打包用）
#   MySQLClient_INCLUDE_DIRS    需要加入的头文件搜索路径（含 vcpkg 的 include/mysql）
#   MySQLClient_LIBRARY         链接库文件（.lib / .so / .a；vcpkg 模式下为导入目标名）
#   MySQLClient_RUNTIME_DLL     Windows 运行时 DLL（可能为空）
#   MySQLClient_VERSION         版本号（可能为空）
#
# 注意：dbpool 的公开头文件使用 #include <mysql.h>，而 vcpkg 把 mysql.h 装在
#       include/mysql 下，所以这里会把“含 mysql.h 的目录”本身也加入
#       INTERFACE_INCLUDE_DIRECTORIES，两种写法都能编译。
# ============================================================================

if(TARGET MySQL::Client)
    set(MySQLClient_FOUND TRUE)
    return()
endif()

# ---------------------------------------------------------------------------
# 0. 基础变量与 vcpkg 安装目录
# ---------------------------------------------------------------------------
if(NOT DEFINED MYSQL_ROOT)
    set(MYSQL_ROOT "$ENV{MYSQL_ROOT}" CACHE PATH "MySQL 安装根目录（包含 include/ 与 lib/），留空则自动查找")
endif()
if(MYSQL_ROOT)
    file(TO_CMAKE_PATH "${MYSQL_ROOT}" MYSQL_ROOT)
endif()

# 从 vcpkg toolchain 推断已安装目录
set(_mysqlclient_vcpkg_installed "")
if(DEFINED VCPKG_INSTALLED_DIR AND VCPKG_TARGET_TRIPLET)
    set(_mysqlclient_vcpkg_installed "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}")
elseif(DEFINED _VCPKG_INSTALLED_DIR AND VCPKG_TARGET_TRIPLET)
    set(_mysqlclient_vcpkg_installed "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}")
endif()

# ---------------------------------------------------------------------------
# 1. vcpkg libmysql 端口（仅在未显式指定 MYSQL_ROOT 时考虑）
# ---------------------------------------------------------------------------
set(_mysqlclient_vcpkg_target "")
if(NOT MYSQL_ROOT)
    find_package(unofficial-libmysql CONFIG QUIET)
    if(TARGET unofficial::libmysql::libmysql)
        set(_mysqlclient_vcpkg_target "unofficial::libmysql::libmysql")
        message(STATUS "MySQL 客户端来源: vcpkg 端口 libmysql")
    endif()
endif()

# ---------------------------------------------------------------------------
# 2. 系统安装路径探测
# ---------------------------------------------------------------------------
set(_mysqlclient_inc_hints "")
set(_mysqlclient_lib_hints "")

if(MYSQL_ROOT)
    list(APPEND _mysqlclient_inc_hints
        "${MYSQL_ROOT}/include" "${MYSQL_ROOT}/include/mysql" "${MYSQL_ROOT}/include/mysql8")
    list(APPEND _mysqlclient_lib_hints
        "${MYSQL_ROOT}/lib" "${MYSQL_ROOT}/lib64" "${MYSQL_ROOT}/lib/vs14"
        "${MYSQL_ROOT}/lib/opt" "${MYSQL_ROOT}/bin")
endif()

if(_mysqlclient_vcpkg_installed)
    list(APPEND _mysqlclient_inc_hints
        "${_mysqlclient_vcpkg_installed}/include"
        "${_mysqlclient_vcpkg_installed}/include/mysql")
    list(APPEND _mysqlclient_lib_hints
        "${_mysqlclient_vcpkg_installed}/lib"
        "${_mysqlclient_vcpkg_installed}/bin")
endif()

# 自动探测常见安装位置（Windows 的 MySQL 默认装在 Program Files 下，路径带版本号）
file(GLOB _mysqlclient_auto_headers
    "C:/Program Files/MySQL/MySQL Server */include/mysql.h"
    "C:/Program Files (x86)/MySQL/MySQL Server */include/mysql.h"
    "D:/Program Files/MySQL/MySQL Server */include/mysql.h"
    "D:/Program Files (x86)/MySQL/MySQL Server */include/mysql.h"
    "E:/Program Files/MySQL/MySQL Server */include/mysql.h"
    "D:/app/MySQL/*/include/mysql.h"
    "D:/app/MySQL/*/include/mysql/mysql.h"
    "C:/MariaDB*/include/mysql.h"
    "D:/MariaDB*/include/mysql.h"
    "C:/xampp/mysql/include/mysql.h"
    "/usr/local/mysql/include/mysql.h"
    "/usr/local/mysql/include/mysql/mysql.h"
    "/opt/homebrew/opt/mysql-client/include/mysql/mysql.h"
    "/usr/local/opt/mysql-client/include/mysql/mysql.h")
# 版本号大的优先
list(SORT _mysqlclient_auto_headers ORDER DESCENDING)

foreach(_header IN LISTS _mysqlclient_auto_headers)
    get_filename_component(_inc_dir "${_header}" DIRECTORY)
    list(APPEND _mysqlclient_inc_hints "${_inc_dir}")
    get_filename_component(_prefix "${_inc_dir}" DIRECTORY)
    if(_prefix MATCHES "/mysql$")
        get_filename_component(_prefix "${_prefix}" DIRECTORY)
    endif()
    list(APPEND _mysqlclient_lib_hints
        "${_prefix}/lib" "${_prefix}/lib64" "${_prefix}/lib/vs14" "${_prefix}/bin")
endforeach()

# ---------------------------------------------------------------------------
# 3. 查找头文件与库
# ---------------------------------------------------------------------------
find_path(MySQLClient_INCLUDE_DIR
    NAMES mysql.h
    HINTS ${_mysqlclient_inc_hints}
    PATHS /usr/include /usr/local/include /usr/local/mysql/include /opt/homebrew/include
    PATH_SUFFIXES mysql mysql8 mariadb
    DOC "包含 mysql.h 的目录")

find_library(MySQLClient_LIBRARY
    NAMES libmysql mysqlclient mysqlclient_r mariadb
    HINTS ${_mysqlclient_lib_hints}
    PATHS /usr/lib /usr/local/lib /usr/lib/x86_64-linux-gnu /usr/local/mysql/lib /opt/homebrew/lib
    PATH_SUFFIXES lib lib64 mariadb
    DOC "MySQL 客户端链接库")

if(WIN32)
    find_file(MySQLClient_RUNTIME_DLL
        NAMES libmysql.dll
        HINTS ${_mysqlclient_lib_hints}
        DOC "MySQL 客户端运行时 DLL")
endif()

# 版本号（从 mysql_version.h 解析）
set(MySQLClient_VERSION "")
if(MySQLClient_INCLUDE_DIR AND EXISTS "${MySQLClient_INCLUDE_DIR}/mysql_version.h")
    file(STRINGS "${MySQLClient_INCLUDE_DIR}/mysql_version.h" _mysqlclient_version_lines
        REGEX "#define[ \t]+MYSQL_VERSION_(MAJOR|MINOR|PATCH)[ \t]+[0-9]+")
    string(REGEX MATCH "MYSQL_VERSION_MAJOR[ \t]+([0-9]+)" _m "${_mysqlclient_version_lines}")
    set(_mysqlclient_version_major "${CMAKE_MATCH_1}")
    string(REGEX MATCH "MYSQL_VERSION_MINOR[ \t]+([0-9]+)" _m "${_mysqlclient_version_lines}")
    set(_mysqlclient_version_minor "${CMAKE_MATCH_1}")
    string(REGEX MATCH "MYSQL_VERSION_PATCH[ \t]+([0-9]+)" _m "${_mysqlclient_version_lines}")
    set(_mysqlclient_version_patch "${CMAKE_MATCH_1}")
    if(_mysqlclient_version_major)
        set(MySQLClient_VERSION
            "${_mysqlclient_version_major}.${_mysqlclient_version_minor}.${_mysqlclient_version_patch}")
    endif()
endif()

# ---------------------------------------------------------------------------
# 4. 组装头文件搜索路径（关键：兼容 include/mysql 布局）
# ---------------------------------------------------------------------------
set(MySQLClient_INCLUDE_DIRS "")
set(_mysqlclient_link_item "")

if(_mysqlclient_vcpkg_target)
    get_target_property(_vcpkg_incs "${_mysqlclient_vcpkg_target}" INTERFACE_INCLUDE_DIRECTORIES)
    if(NOT _vcpkg_incs OR _vcpkg_incs MATCHES "-NOTFOUND$")
        set(_vcpkg_incs "")
    endif()

    foreach(_inc IN LISTS _vcpkg_incs)
        if(IS_DIRECTORY "${_inc}")
            list(APPEND MySQLClient_INCLUDE_DIRS "${_inc}")
        endif()
    endforeach()

    # 兜底：直接使用 vcpkg 安装目录下的 include
    if(NOT MySQLClient_INCLUDE_DIRS AND _mysqlclient_vcpkg_installed
       AND IS_DIRECTORY "${_mysqlclient_vcpkg_installed}/include")
        list(APPEND MySQLClient_INCLUDE_DIRS "${_mysqlclient_vcpkg_installed}/include")
    endif()

    foreach(_inc IN LISTS MySQLClient_INCLUDE_DIRS)
        if(EXISTS "${_inc}/mysql/mysql.h")
            list(APPEND MySQLClient_INCLUDE_DIRS "${_inc}/mysql")
            # 链接的是 vcpkg 目标，打包时也应当带上同一套头文件
            set(MySQLClient_INCLUDE_DIR "${_inc}/mysql")
        endif()
    endforeach()

    # 让扁平交付包也能带上 DLL / 导入库
    if(_mysqlclient_vcpkg_installed)
        if(WIN32 AND NOT MySQLClient_RUNTIME_DLL
           AND EXISTS "${_mysqlclient_vcpkg_installed}/bin/libmysql.dll")
            set(MySQLClient_RUNTIME_DLL "${_mysqlclient_vcpkg_installed}/bin/libmysql.dll")
        endif()
        if(NOT MySQLClient_LIBRARY AND EXISTS "${_mysqlclient_vcpkg_installed}/lib/libmysql.lib")
            set(MySQLClient_LIBRARY "${_mysqlclient_vcpkg_installed}/lib/libmysql.lib")
        endif()
    endif()

    set(_mysqlclient_link_item "${_mysqlclient_vcpkg_target}")

elseif(MySQLClient_INCLUDE_DIR AND MySQLClient_LIBRARY)
    list(APPEND MySQLClient_INCLUDE_DIRS "${MySQLClient_INCLUDE_DIR}")
    # 有的发行版把 mysql.h 放在 include/mysql，此时再补上父目录以支持 <mysql/mysql.h>
    get_filename_component(_parent "${MySQLClient_INCLUDE_DIR}" DIRECTORY)
    if(EXISTS "${_parent}/mysql/mysql.h")
        list(APPEND MySQLClient_INCLUDE_DIRS "${_parent}")
    endif()
    set(_mysqlclient_link_item "${MySQLClient_LIBRARY}")
endif()

list(REMOVE_DUPLICATES MySQLClient_INCLUDE_DIRS)

# ---------------------------------------------------------------------------
# 5. 结果校验与导入目标
# ---------------------------------------------------------------------------
include(FindPackageHandleStandardArgs)

set(_mysqlclient_required_vars MySQLClient_INCLUDE_DIR MySQLClient_LIBRARY)
if(_mysqlclient_vcpkg_target)
    set(_mysqlclient_required_vars MySQLClient_INCLUDE_DIRS)
endif()

find_package_handle_standard_args(MySQLClient
    REQUIRED_VARS ${_mysqlclient_required_vars}
    VERSION_VAR MySQLClient_VERSION
    REASON_FAILURE_MESSAGE "未找到 MySQL 客户端开发库（mysql.h / libmysql.lib）。请安装 MySQL 开发组件，或指定安装根目录：cmake -B build -DMYSQL_ROOT=\"C:/Program Files/MySQL/MySQL Server 8.0\"；也可以在 vcpkg 中安装 libmysql 端口后重新配置。")

if(MySQLClient_FOUND)
    set(MySQLClient_LIBRARIES "${_mysqlclient_link_item}")
    if(NOT TARGET MySQL::Client)
        add_library(MySQL::Client INTERFACE IMPORTED)
        set_target_properties(MySQL::Client PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${MySQLClient_INCLUDE_DIRS}"
            INTERFACE_LINK_LIBRARIES "${_mysqlclient_link_item}")
    endif()
endif()

mark_as_advanced(MySQLClient_INCLUDE_DIR MySQLClient_LIBRARY MySQLClient_RUNTIME_DLL)
