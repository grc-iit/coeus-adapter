add_executable(mpi_test mpit_tests.cpp)

target_link_libraries(mpi_test PRIVATE
        GTest::gtest_main
        GTest::gmock_main
        MPI::MPI_CXX
        ${Hermes_LIBRARIES}
        ${ADIOS2_LIBRARIES}
        spdlog
        cereal
        rapidjson)

target_include_directories(mpi_test PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${Hermes_INCLUDE_DIRS}
        ${ADIOS2_INCLUDE_DIR})