add_executable(basic ${CMAKE_CURRENT_SOURCE_DIR}/basic.cc)
add_dependencies(basic hermes_engine)
target_link_libraries(basic PRIVATE hermes_engine MPI::MPI_CXX)

function(pytest test_type test_name)
   set(script ${CMAKE_SOURCE_DIR}/CI/py_coeus_ci/bin/run_test)
   set(HERMES_USE_ADDRESS_SANITIZER FALSE)
   add_test(NAME ${test_name}
         COMMAND ${script} ${test_type} ${test_name} ${CMAKE_BINARY_DIR} ${HERMES_USE_ADDRESS_SANITIZER})
   set_tests_properties(${test_name} PROPERTIES ENVIRONMENT
           "LD_LIBRARY_PATH=${COEUS_BINARY_DIR}/bin:$ENV{LD_LIBRARY_PATH}")
endfunction()