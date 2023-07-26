add_executable(basic basic.cc)
add_dependencies(basic hermes_engine)
target_link_libraries(basic PRIVATE hermes_engine MPI::MPI_CXX)
#pytest(native test_basic)
