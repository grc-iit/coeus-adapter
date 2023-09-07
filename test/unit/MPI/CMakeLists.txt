add_executable(mpi_test mpit_tests.cpp)

target_link_libraries(mpi_test
        GTest::gtest_main
        GTest::gmock_main
        MPI::MPI_CXX)

target_include_directories(mpi_test PRIVATE
        ${PROJECT_SOURCE_DIR}/include)