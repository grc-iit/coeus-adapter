add_executable(basic ${CMAKE_CURRENT_SOURCE_DIR}/basic.cc)
add_executable(put_test ${CMAKE_CURRENT_SOURCE_DIR}/put_test.cc)
add_executable(get_test ${CMAKE_CURRENT_SOURCE_DIR}/get_test.cc)
add_executable(basic_three_vars ${CMAKE_CURRENT_SOURCE_DIR}/basic_three_vars.cc)
add_dependencies(basic hermes_engine)
add_dependencies(put_test hermes_engine)
add_dependencies(get_test hermes_engine)
add_dependencies(basic_three_vars hermes_engine)
target_link_libraries(basic PRIVATE hermes_engine MPI::MPI_CXX)
target_link_libraries(put_test PRIVATE hermes_engine MPI::MPI_CXX)
target_link_libraries(get_test PRIVATE hermes_engine MPI::MPI_CXX)
target_link_libraries(basic_three_vars PRIVATE hermes_engine MPI::MPI_CXX)

pytest(native test_basic)
pytest(native test_basic_three_vars)
pytest(native test_put_and_get_test)

set_tests_properties(${test_name} PROPERTIES ENVIRONMENT
            "LD_LIBRARY_PATH=${COEUS_BINARY_DIR}/bin:$ENV{LD_LIBRARY_PATH}")