# 冒烟测试：链接 dbpool，验证导出符号与头文件可用
add_executable(dbpool_smoke smoke.cpp)
target_link_libraries(dbpool_smoke PRIVATE dbpool)

# 运行期把动态库和依赖 DLL 放到 exe 旁边
if(WIN32)
    add_custom_command(TARGET dbpool_smoke POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:dbpool>" "$<TARGET_FILE_DIR:dbpool_smoke>"
        VERBATIM)

    add_custom_command(TARGET dbpool_smoke POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:Utils::Utils>" "$<TARGET_FILE_DIR:dbpool_smoke>"
        VERBATIM)

    if(MySQLClient_RUNTIME_DLL)
        add_custom_command(TARGET dbpool_smoke POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${MySQLClient_RUNTIME_DLL}" "$<TARGET_FILE_DIR:dbpool_smoke>"
            VERBATIM)
    endif()
endif()

add_test(NAME dbpool_smoke COMMAND dbpool_smoke)
