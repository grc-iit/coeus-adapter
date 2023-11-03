MESSAGE(STATUS "Building hermes_mocks")

add_library(hermes_mocks INTERFACE
        MockBucket.h
        MockHermes.h)
target_include_directories(hermes_mocks INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include/comms/interfaces
        ${Hermes_INCLUDE_DIRS})

target_link_libraries(hermes_mocks INTERFACE
        GTest::gtest_main
        GTest::gmock_main
        ${Hermes_LIBRARIES})

add_library(mpi_mocks INTERFACE
        MockMPI.h)
target_include_directories(mpi_mocks INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include/comms/interfaces)

target_link_libraries(mpi_mocks INTERFACE
        GTest::gtest_main
        GTest::gmock_main
        MPI::MPI_CXX)