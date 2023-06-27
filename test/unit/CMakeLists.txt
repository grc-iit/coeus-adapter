add_executable(basic ${CMAKE_CURRENT_SOURCE_DIR}/basic.cc)
add_dependencies(basic hermes_engine)
target_link_libraries(basic PRIVATE hermes_engine MPI::MPI_CXX)

pytest(native test_basic)

set_tests_properties(${test_name} PROPERTIES ENVIRONMENT
            "LD_LIBRARY_PATH=${COEUS_BINARY_DIR}/bin:$ENV{LD_LIBRARY_PATH}")