add_executable(sqlite_base_test base.cpp)

target_include_directories(sqlite_base_test PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${SQLite3_INCLUDE_DIRS}
        ${Hermes_INCLUDE_DIRS}
        ${ADIOS2_INCLUDE_DIR})
target_link_libraries(sqlite_base_test
        GTest::gtest_main
        GTest::gmock_main
        ${SQLite3_LIBRARIES}
        ${Hermes_LIBRARIES}
        ${ADIOS2_LIBRARIES}
        spdlog
        cereal
        rapidjson)
#gtest_discover_tests(sqlite_base_test)