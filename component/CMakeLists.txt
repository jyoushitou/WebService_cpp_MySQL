# ============================================================================
# component/CMakeLists.txt —— dbpool 数据库连接池组件
#
# 产物：
#   · dbpool 动态库 / 静态库（DBPOOL_BUILD_SHARED 控制）
#   · 可被 find_package(dbpool CONFIG) 使用的 CMake 包（install 后）
#   · 可选：扁平交付目录（.h 与 .lib/.so 在同一个文件夹里，复制即用）
# ============================================================================

get_filename_component(DBPOOL_CMAKE_MODULE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../cmake" ABSOLUTE)
if(NOT "${DBPOOL_CMAKE_MODULE_DIR}" IN_LIST CMAKE_MODULE_PATH)
    list(APPEND CMAKE_MODULE_PATH "${DBPOOL_CMAKE_MODULE_DIR}")
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# 包版本由顶层 CMakeLists.txt 传入（DBPOOL_VERSION = PROJECT_VERSION）
if(NOT DEFINED DBPOOL_VERSION)
    set(DBPOOL_VERSION "1.0.0")
endif()

# 本文件由顶层 CMakeLists.txt 引入；下面的选项在顶层已定义，这里只是兜底，
# 保证“把 component/ 单独加入其它工程”时也能正常配置。
if(NOT DEFINED DBPOOL_BUILD_SHARED)
    option(DBPOOL_BUILD_SHARED "构建动态库（.dll/.so）；OFF 则构建静态库" ON)
endif()
if(NOT DEFINED DBPOOL_BUILD_BUNDLE)
    option(DBPOOL_BUILD_BUNDLE "生成扁平交付目录（.h 与 .lib/.so 同目录）" ON)
endif()
if(NOT DEFINED DBPOOL_BUNDLE_WITH_MYSQL)
    option(DBPOOL_BUNDLE_WITH_MYSQL "把 MySQL 客户端头文件/DLL/导入库一并放进交付目录" ON)
endif()
if(NOT DEFINED DBPOOL_BUNDLE_WITH_UTILS)
    option(DBPOOL_BUNDLE_WITH_UTILS "把 Utils 组件的头文件/库一并放进交付目录" ON)
endif()

# ---------------------------------------------------------------------------
# 依赖
#   Utils：统一的日志/IO 组件，作为独立 vcpkg 包（目标 Utils::Utils）提供
# ---------------------------------------------------------------------------
find_package(MySQLClient REQUIRED)
find_package(Utils CONFIG REQUIRED)
find_package(Threads REQUIRED)

# ---------------------------------------------------------------------------
# 目标定义
# ---------------------------------------------------------------------------
set(DBPOOL_PUBLIC_HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/MySQL_Pool.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/MySQL_Connection.h")

set(DBPOOL_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/source/MySQL_Connection.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/MySQL_Pool.cpp")

if(DBPOOL_BUILD_SHARED)
    add_library(dbpool SHARED ${DBPOOL_SOURCES} ${DBPOOL_PUBLIC_HEADERS})
    set(_dbpool_library_type "SHARED")
else()
    add_library(dbpool STATIC ${DBPOOL_SOURCES} ${DBPOOL_PUBLIC_HEADERS})
    set(_dbpool_library_type "STATIC")
endif()

set_target_properties(dbpool PROPERTIES
    OUTPUT_NAME "dbpool"
    EXPORT_NAME "dbpool"
    POSITION_INDEPENDENT_CODE ON)

# MSVC 下不写 __declspec(dllexport) 也导出全部符号：
# 这样“复制 .h + .lib/.dll”的用户无需额外宏定义即可直接链接。
if(DBPOOL_BUILD_SHARED AND WIN32 AND MSVC)
    set_target_properties(dbpool PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

target_compile_features(dbpool PUBLIC cxx_std_17)

target_include_directories(dbpool
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

target_link_libraries(dbpool
    PUBLIC MySQL::Client Utils::Utils
    PRIVATE Threads::Threads)

target_compile_definitions(dbpool PRIVATE
    $<$<BOOL:${WIN32}>:WIN32_LEAN_AND_MEAN>
    $<$<BOOL:${WIN32}>:NOMINMAX>)

# 源码里有中文注释，MSVC 需要按 UTF-8 解析
if(MSVC)
    target_compile_options(dbpool PRIVATE /utf-8)
endif()

# ---------------------------------------------------------------------------
# 安装与 CMake 包导出（vcpkg 端口依赖这部分）
#   <prefix>/include/MySQL_Pool.h
#   <prefix>/lib/dbpool.lib | <prefix>/lib/libdbpool.so
#   <prefix>/lib/cmake/dbpool/{dbpoolConfig.cmake, dbpoolTargets.cmake, FindMySQLClient.cmake}
# ---------------------------------------------------------------------------
set(DBPOOL_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/dbpool"
    CACHE STRING "dbpool CMake 包配置安装目录")

install(TARGETS dbpool
    EXPORT dbpoolTargets
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

install(FILES ${DBPOOL_PUBLIC_HEADERS}
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

install(EXPORT dbpoolTargets
    FILE dbpoolTargets.cmake
    NAMESPACE dbpool::
    DESTINATION "${DBPOOL_INSTALL_CMAKEDIR}")

# 把 FindMySQLClient.cmake 一起装出去，消费者无需自己安装
install(FILES "${DBPOOL_CMAKE_MODULE_DIR}/FindMySQLClient.cmake"
    DESTINATION "${DBPOOL_INSTALL_CMAKEDIR}")

configure_package_config_file(
    "${DBPOOL_CMAKE_MODULE_DIR}/dbpoolConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/dbpoolConfig.cmake"
    INSTALL_DESTINATION "${DBPOOL_INSTALL_CMAKEDIR}")

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/dbpoolConfigVersion.cmake"
    VERSION "${DBPOOL_VERSION}"
    COMPATIBILITY SameMajorVersion)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/dbpoolConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/dbpoolConfigVersion.cmake"
    DESTINATION "${DBPOOL_INSTALL_CMAKEDIR}")

# pkg-config（主要方便 Linux 上直接 g++ 链接）
if(UNIX)
    configure_file("${DBPOOL_CMAKE_MODULE_DIR}/dbpool.pc.in"
        "${CMAKE_CURRENT_BINARY_DIR}/dbpool.pc" @ONLY)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/dbpool.pc"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")
endif()

# ---------------------------------------------------------------------------
# 扁平交付目录：头文件（复制）+ 库（编译产物）放在同一个文件夹
# ---------------------------------------------------------------------------
if(DBPOOL_BUILD_BUNDLE)
    if(NOT DBPOOL_BUNDLE_DIR)
        set(DBPOOL_BUNDLE_DIR "${CMAKE_BINARY_DIR}/dbpool-${DBPOOL_VERSION}"
            CACHE PATH "扁平交付目录（.h 与 .lib/.so 同目录）")
    endif()

    set(_bundle_files ${DBPOOL_PUBLIC_HEADERS})
    if(DBPOOL_BUILD_SHARED)
        list(APPEND _bundle_files
            "$<TARGET_FILE:dbpool>"
            "$<TARGET_LINKER_FILE:dbpool>")
    else()
        list(APPEND _bundle_files "$<TARGET_FILE:dbpool>")
    endif()

    set(_bundle_mysql_files "")
    set(_bundle_mysql_subfiles "")
    if(DBPOOL_BUNDLE_WITH_MYSQL)
        # MySQL 客户端头文件：让“复制即用”的目录不需要再装 MySQL 开发包
        if(MySQLClient_INCLUDE_DIR AND IS_DIRECTORY "${MySQLClient_INCLUDE_DIR}")
            file(GLOB _mysql_client_headers "${MySQLClient_INCLUDE_DIR}/*.h")
            list(APPEND _bundle_mysql_files ${_mysql_client_headers})

            # mysql.h / mysql_com.h 里有 "mysql/xxx.h" 这类相对包含，
            # 子目录需要按原结构保留到 <交付目录>/mysql/ 下
            if(IS_DIRECTORY "${MySQLClient_INCLUDE_DIR}/mysql")
                file(GLOB_RECURSE _mysql_client_subheaders
                    "${MySQLClient_INCLUDE_DIR}/mysql/*.h")
                list(APPEND _bundle_mysql_subfiles ${_mysql_client_subheaders})
            endif()
        endif()
        # Windows 下还需要 DLL 与导入库
        if(WIN32)
            if(MySQLClient_RUNTIME_DLL AND EXISTS "${MySQLClient_RUNTIME_DLL}")
                list(APPEND _bundle_mysql_files "${MySQLClient_RUNTIME_DLL}")
            endif()
            if(MySQLClient_LIBRARY AND EXISTS "${MySQLClient_LIBRARY}")
                list(APPEND _bundle_mysql_files "${MySQLClient_LIBRARY}")
            endif()
        endif()
    endif()

    # Utils 组件（vcpkg 包 utils）：头文件 + 库 + DLL，保证交付目录自包含
    set(_bundle_utils_files "")
    if(DBPOOL_BUNDLE_WITH_UTILS)
        get_target_property(_utils_include_dirs Utils::Utils INTERFACE_INCLUDE_DIRECTORIES)
        if(_utils_include_dirs)
            foreach(_dir IN LISTS _utils_include_dirs)
                foreach(_name IN ITEMS Utils.h Message.h UtilsExport.h)
                    if(EXISTS "${_dir}/${_name}")
                        list(APPEND _bundle_utils_files "${_dir}/${_name}")
                    endif()
                endforeach()
            endforeach()
        endif()

        if(WIN32)
            list(APPEND _bundle_utils_files
                "$<TARGET_FILE:Utils::Utils>"
                "$<TARGET_LINKER_FILE:Utils::Utils>")
        else()
            list(APPEND _bundle_utils_files "$<TARGET_FILE:Utils::Utils>")
        endif()
    endif()
    list(REMOVE_DUPLICATES _bundle_utils_files)

    # 用 manifest 传递（多配置生成器下 $<CONFIG> 各异）
    set(_bundle_manifest "${CMAKE_CURRENT_BINARY_DIR}/dbpool-bundle-$<CONFIG>.cmake")
    file(GENERATE OUTPUT "${_bundle_manifest}" CONTENT
"set(DBPOOL_BUNDLE_DIR [==[${DBPOOL_BUNDLE_DIR}]==])
set(DBPOOL_BUNDLE_FILES [==[${_bundle_files}]==])
set(DBPOOL_BUNDLE_MYSQL_FILES [==[${_bundle_mysql_files}]==])
set(DBPOOL_BUNDLE_MYSQL_SUBDIR [==[mysql]==])
set(DBPOOL_BUNDLE_MYSQL_SUBFILES [==[${_bundle_mysql_subfiles}]==])
set(DBPOOL_BUNDLE_UTILS_FILES [==[${_bundle_utils_files}]==])
set(DBPOOL_BUNDLE_VERSION [==[${DBPOOL_VERSION}]==])
set(DBPOOL_BUNDLE_LINKAGE [==[${_dbpool_library_type}]==])
")

    add_custom_target(dbpool_bundle ALL
        COMMAND "${CMAKE_COMMAND}"
            "-DDBPOOL_BUNDLE_MANIFEST=${_bundle_manifest}"
            -P "${DBPOOL_CMAKE_MODULE_DIR}/BundleDbpool.cmake"
        DEPENDS dbpool ${DBPOOL_PUBLIC_HEADERS} ${_bundle_mysql_files}
        COMMENT "刷新扁平交付目录：${DBPOOL_BUNDLE_DIR}"
        VERBATIM)
endif()

# ---------------------------------------------------------------------------
# 冒烟测试（与 Utils 一致：tests/ 放在组件目录下）
# ---------------------------------------------------------------------------
if(DBPOOL_BUILD_TESTS)
    add_subdirectory(tests)
endif()

# ---------------------------------------------------------------------------
# 摘要
# ---------------------------------------------------------------------------
message(STATUS "")
message(STATUS "  dbpool ${DBPOOL_VERSION} (${_dbpool_library_type})")
message(STATUS "    MySQL  : ${MySQLClient_LIBRARY} ${MySQLClient_VERSION}")
message(STATUS "    头文件 : ${CMAKE_CURRENT_SOURCE_DIR}/include")
if(DBPOOL_BUILD_BUNDLE)
    message(STATUS "    交付包 : ${DBPOOL_BUNDLE_DIR}")
endif()
message(STATUS "    安装后 : find_package(dbpool CONFIG REQUIRED) -> dbpool::dbpool")
message(STATUS "")
