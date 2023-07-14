add_executable(metadata metadata.cc)
add_dependencies(metadata hermes_engine)
target_link_libraries(metadata PRIVATE hermes_engine MPI::MPI_CXX)

add_test(NAME metadata_test
        COMMAND metadata)
set_tests_properties(metadata_test PROPERTIES
        ENVIRONMENT "LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/bin:$ENV{LD_LIBRARY_PATH}")
