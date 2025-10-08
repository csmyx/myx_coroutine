set(MYX_COROUTINE_TEST ${LIB_NAME}_test)

file(GLOB test_src "*.cpp")
add_executable(${MYX_COROUTINE_TEST} ${test_src})
target_link_libraries(${MYX_COROUTINE_TEST} 
    PRIVATE 
        ${LIB_NAME}
        GTest::gtest GTest::gtest_main GTest::gmock GTest::gmock_main
)

add_test(NAME run_myx_coroutine_test COMMAND ${MYX_COROUTINE_TEST})


