MESSAGE(STATUS "This is BINARY dir " ${PROJECT_BINARY_DIR})
add_executable(json_tests test.cpp)

target_include_directories(json_tests PRIVATE
        ${PROJECT_SOURCE_DIR}/include)

target_link_libraries(json_tests PRIVATE
        GTest::gtest_main
        rapidjson)