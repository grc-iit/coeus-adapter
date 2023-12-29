add_executable(yaml_test test.cpp)

target_include_directories(yaml_test PRIVATE
        ${PROJECT_SOURCE_DIR}/include)

#message(STATUS "Used config file: ${yaml-cpp_CONFIG}")
#message(STATUS "Used include: ${YAML_CPP_INCLUDE_DIR}")
#message(STATUS "Used lib: ${YAML_CPP_LIBRARIES}")

target_link_libraries(yaml_test PRIVATE
        GTest::gtest_main
        yaml-cpp)