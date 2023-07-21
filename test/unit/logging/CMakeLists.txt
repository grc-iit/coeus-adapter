add_executable(logger_test logger_test.cc)
add_dependencies(logger_test hermes_engine)
target_link_libraries(logger_test PRIVATE hermes_engine MPI::MPI_CXX)


add_test(NAME logger_test
        COMMAND logger_test)

set_tests_properties(logger_test PROPERTIES
        ENVIRONMENT "LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/bin:$ENV{LD_LIBRARY_PATH}")
