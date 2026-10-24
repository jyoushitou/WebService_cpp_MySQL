# ============================================================================
# ports/dbpool/portfile.cmake —— dbpool 的 vcpkg 覆盖端口
#
# 本地安装（可直接使用当前工作区源码，未提交也能装）：
#   vcpkg install dbpool:x64-windows --overlay-ports=<本仓库>/ports
#
# 其他工程引用：
#   vcpkg.json  ->  "dependencies": ["dbpool"]
#   CMakeLists  ->  find_package(dbpool CONFIG REQUIRED)
#                   target_link_libraries(app PRIVATE dbpool::dbpool)
#
# 若要发布到自己的注册表，请把下面的“本地源码”分支删掉，改为
# vcpkg_from_git/vcpkg_from_github 并固定 REF（当前占位为仓库提交号）。
# ============================================================================
set(_local_source "${CURRENT_PORT_DIR}/../../..")

if(EXISTS "${_local_source}/component/CMakeLists.txt")
    # 覆盖端口调试：直接使用本仓库工作区
    set(SOURCE_PATH "${_local_source}")
    message(STATUS "dbpool: 使用本地源码目录 ${SOURCE_PATH}")
else()
    vcpkg_from_git(
        OUT_SOURCE_PATH SOURCE_PATH
        URL https://github.com/jyoushitou/cpp_MySQL.git
        REF 5217d8770b0b0f5b1c0b8ad2e4b4e0b3f7e0a000 # TODO: 发布时改成真实提交号
    )
endif()

string(COMPARE EQUAL "${VCPKG_LIBRARY_LINKAGE}" "dynamic" _dbpool_shared)
if(_dbpool_shared)
    set(_dbpool_shared ON)
else()
    set(_dbpool_shared OFF)
endif()

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DDBPOOL_BUILD_SHARED=${_dbpool_shared}
        -DDBPOOL_BUILD_BUNDLE=OFF
        -DDBPOOL_BUILD_TESTS=OFF
        -DDBPOOL_BUILD_MYSQL_SERVICE=OFF
        -DDBPOOL_BUNDLE_WITH_MYSQL=OFF
    MAYBE_UNUSED_VARIABLES
        DBPOOL_BUILD_BUNDLE
        DBPOOL_BUNDLE_WITH_MYSQL)

vcpkg_cmake_install()

# 把 lib/cmake/dbpool 移到 share/dbpool（同时修正绝对路径）
vcpkg_cmake_config_fixup(CONFIG_PATH lib/cmake/dbpool)

vcpkg_copy_pdbs()

file(REMOVE_RECURSE
    "${CURRENT_PACKAGES_DIR}/debug/include"
    "${CURRENT_PACKAGES_DIR}/debug/share")

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")

file(INSTALL "${CMAKE_CURRENT_LIST_DIR}/usage"
    DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}")
