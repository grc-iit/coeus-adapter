add_executable(hello_test gtest.cpp)

target_link_libraries(hello_test
        GTest::gtest_main
        GTest::gmock_main
        hermes_mocks)

include(GoogleTest)
gtest_discover_tests(hello_test)