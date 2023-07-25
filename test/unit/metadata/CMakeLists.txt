add_executable(metadata metadata.cc)
add_dependencies(metadata hermes_engine)
target_link_libraries(metadata PRIVATE hermes_engine MPI::MPI_CXX)

add_executable(split_metadata_put put_metadata.cc)
add_dependencies(split_metadata_put hermes_engine)
target_link_libraries(split_metadata_put PRIVATE hermes_engine MPI::MPI_CXX)

add_executable(split_metadata_get get_metadata.cc)
add_dependencies(split_metadata_get hermes_engine)
target_link_libraries(split_metadata_get PRIVATE hermes_engine MPI::MPI_CXX)


add_test(NAME metadata_test
          COMMAND metadata)

pytest(native test_split_metadata)

set_tests_properties(metadata_test PROPERTIES
        ENVIRONMENT "LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/bin:$ENV{LD_LIBRARY_PATH}")
