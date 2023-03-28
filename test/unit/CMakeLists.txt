add_executable(basic ${CMAKE_CURRENT_SOURCE_DIR}/basic.cc)
add_dependencies(basic hermes_engine)
target_link_libraries(basic
        hermes_engine MPI::MPI_CXX ${ADIOS2_LIBRARIES})
