add_executable(derived_debug test.cpp)
target_link_libraries(derived_debug PRIVATE ${ADIOS2_LIBRARIES} MPI::MPI_CXX)
target_include_directories(derived_debug PRIVATE ${ADIOS2_INCLUDE_DIR})
