add_executable(adios_derived test.cpp)
target_link_libraries(adios_derived PRIVATE ${ADIOS2_LIBRARIES} MPI::MPI_CXX)
target_include_directories(adios_derived PRIVATE ${ADIOS2_INCLUDE_DIR})
