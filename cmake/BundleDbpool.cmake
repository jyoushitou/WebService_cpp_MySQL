# ============================================================================
# BundleDbpool.cmake
#   由 add_custom_target(dbpool_bundle) 以 -P 方式调用：
#   把清单中的头文件与库文件复制到同一个目录（扁平交付，复制即用）。
# ============================================================================
cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED DBPOOL_BUNDLE_MANIFEST)
    message(FATAL_ERROR "缺少参数 -DDBPOOL_BUNDLE_MANIFEST=<manifest.cmake>")
endif()

include("${DBPOOL_BUNDLE_MANIFEST}")

file(MAKE_DIRECTORY "${DBPOOL_BUNDLE_DIR}")

# 清理上一次（另一种链接方式）留下的 dbpool 库文件，避免静态库与导入库混在一起
file(GLOB _stale_artifacts
    "${DBPOOL_BUNDLE_DIR}/dbpool.dll"
    "${DBPOOL_BUNDLE_DIR}/dbpool.lib"
    "${DBPOOL_BUNDLE_DIR}/dbpool.exp"
    "${DBPOOL_BUNDLE_DIR}/dbpool.pdb"
    "${DBPOOL_BUNDLE_DIR}/libdbpool.so*"
    "${DBPOOL_BUNDLE_DIR}/libdbpool.a")
if(_stale_artifacts)
    file(REMOVE ${_stale_artifacts})
endif()

set(_copied 0)
foreach(_file IN LISTS DBPOOL_BUNDLE_FILES DBPOOL_BUNDLE_MYSQL_FILES DBPOOL_BUNDLE_UTILS_FILES)
    if(NOT EXISTS "${_file}")
        message(WARNING "dbpool_bundle: 跳过不存在的文件 ${_file}")
        continue()
    endif()

    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${_file}" "${DBPOOL_BUNDLE_DIR}/"
        RESULT_VARIABLE _result)
    if(NOT _result EQUAL 0)
        message(FATAL_ERROR "dbpool_bundle: 复制失败 ${_file}")
    endif()

    math(EXPR _copied "${_copied} + 1")
endforeach()

# 保留 mysql/ 子目录结构（mysql.h / mysql_com.h 内部有相对包含）
if(DBPOOL_BUNDLE_MYSQL_SUBFILES)
    set(_subdir "${DBPOOL_BUNDLE_DIR}/${DBPOOL_BUNDLE_MYSQL_SUBDIR}")
    file(MAKE_DIRECTORY "${_subdir}")
    foreach(_file IN LISTS DBPOOL_BUNDLE_MYSQL_SUBFILES)
        if(NOT EXISTS "${_file}")
            continue()
        endif()
        execute_process(
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${_file}" "${_subdir}/"
            RESULT_VARIABLE _result)
        if(NOT _result EQUAL 0)
            message(FATAL_ERROR "dbpool_bundle: 复制失败 ${_file}")
        endif()
        math(EXPR _copied "${_copied} + 1")
    endforeach()
endif()

if(DBPOOL_BUNDLE_LINKAGE STREQUAL "SHARED")
    set(_link_hint "dbpool.lib  （Windows 导入库）或 -ldbpool（Linux）")
    set(_runtime_hint "运行期需要 dbpool.dll 与本 exe 同目录，或位于 PATH 中；Linux 需把本目录加入 LD_LIBRARY_PATH")
else()
    set(_link_hint "dbpool.lib / libdbpool.a（静态库）")
    set(_runtime_hint "静态链接，无需运行期 DLL")
endif()

file(WRITE "${DBPOOL_BUNDLE_DIR}/README.txt"
"dbpool ${DBPOOL_BUNDLE_VERSION} —— MySQL 连接池组件（扁平交付目录）

本目录同时包含头文件与链接库，整个目录复制到任意位置即可使用。

目录内容
--------
  MySQL_Pool.h        连接池接口
  MySQL_Connection.h  单条 MySQL 连接接口
  ${_link_hint}
  mysql.h / mysql_com.h / ...         MySQL 客户端头文件（如构建时可用）
  mysql/                              上述头文件内部相对包含的子目录
  libmysql.lib / libmysql.dll          MySQL 客户端库
  Utils.h / Message.h / Utils.lib / Utils.dll   统一日志组件（如构建时可用）
  ${_runtime_hint}

直接编译（不使用 CMake）
------------------------
  Windows (MSVC):
    cl /std:c++17 /EHsc /I. your.cpp /link dbpool.lib libmysql.lib

  Linux (GCC/Clang):
    g++ -std=c++17 -I. your.cpp -L. -ldbpool -lmysqlclient -lpthread

示例代码
--------
  #include \"MySQL_Pool.h\"

  int main()
  {
      Sql::MySQLPool* pool = Sql::MySQLPool::GetConnectionPool();
      if (!pool->init(\"127.0.0.1\", 3306, \"root\", \"password\", \"test\",
                       4, 16, 60, 5, 1000))
      {
          return 1;
      }

      Sql::Connection* conn = pool->GetConnection();
      // ... 使用 conn ...
      pool->ReConnection(conn);
      pool->shutdown();
      return 0;
  }

使用 CMake（推荐）
------------------
  先安装：cmake --install <build-dir> --prefix <install-dir>
  再引用：
    find_package(dbpool CONFIG REQUIRED)
    target_link_libraries(app PRIVATE dbpool::dbpool)

注意
----
  dbpool 只负责连接池管理，MySQL 客户端库（libmysql）仍按其自身许可证分发。
")

message(STATUS "dbpool_bundle: 已刷新 ${DBPOOL_BUNDLE_DIR}（${_copied} 个文件）")
