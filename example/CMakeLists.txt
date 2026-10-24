# ============================================================================
# example/CMakeLists.txt —— dbpool 独立演示程序
# 直接运行即可验证「头文件 + 库」是否可用：
#   cmake --build build --config Release --target dbpool_demo
#   build/example/Release/dbpool_demo.exe --help
# ============================================================================

add_executable(dbpool_demo main.cpp)
target_link_libraries(dbpool_demo PRIVATE dbpool)

# 运行期把依赖 DLL 放到 exe 旁边
if(WIN32)
    add_custom_command(TARGET dbpool_demo POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:dbpool>" "$<TARGET_FILE:Utils::Utils>"
            "$<TARGET_FILE_DIR:dbpool_demo>"
        VERBATIM)

    if(MySQLClient_RUNTIME_DLL)
        add_custom_command(TARGET dbpool_demo POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${MySQLClient_RUNTIME_DLL}" "$<TARGET_FILE_DIR:dbpool_demo>"
            VERBATIM)
    endif()
endif()
