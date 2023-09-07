add_executable(sqlite_base_test base.cpp)

target_include_directories(sqlite_base_test PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${SQLite3_INCLUDE_DIRS}
        )
target_link_libraries(sqlite_base_test
        GTest::gtest_main
        GTest::gmock_main
        ${SQLite3_LIBRARIES}
        )
gtest_discover_tests(sqlite_base_test)