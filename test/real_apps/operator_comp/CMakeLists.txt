add_executable(operator_comp_producer operator_producer.cpp)
target_link_libraries(operator_comp_producer PRIVATE
        ${ADIOS2_LIBRARIES}
        spdlog
        cereal
        rapidjson
        MPI::MPI_CXX)
target_include_directories(operator_comp_producer PRIVATE
        ${Hermes_INCLUDE_DIRS}
        ${ADIOS2_INCLUDE_DIR})

add_executable(operator_comp_consumer operator_consumer.cpp)
target_link_libraries(operator_comp_consumer PRIVATE
        ${ADIOS2_LIBRARIES}
        spdlog
        cereal
        rapidjson
        MPI::MPI_CXX)
target_include_directories(operator_comp_consumer PRIVATE
        ${Hermes_INCLUDE_DIRS}
        ${ADIOS2_INCLUDE_DIR})
