add_executable(io_comp io_comp.cpp)
target_link_libraries(io_comp PRIVATE
        ${ADIOS2_LIBRARIES}
        spdlog
        cereal
        rapidjson
        MPI::MPI_CXX)
target_include_directories(io_comp PRIVATE
        ${Hermes_INCLUDE_DIRS}
        ${ADIOS2_INCLUDE_DIR})