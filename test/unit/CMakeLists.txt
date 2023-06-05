add_executable(basic ${CMAKE_CURRENT_SOURCE_DIR}/basic.cc)
add_dependencies(basic hermes_engine)
target_link_libraries(basic PRIVATE hermes_engine MPI::MPI_CXX)

add_test(NAME basic_plugin_test
         COMMAND ${CMAKE_BINARY_DIR}/bin/basic ${CMAKE_BINARY_DIR}/bin)
set_tests_properties(basic_plugin_test PROPERTIES ENVIRONMENT
        "LD_LIBRARY_PATH=${COEUS_BINARY_DIR}/bin:$ENV{LD_LIBRARY_PATH}")