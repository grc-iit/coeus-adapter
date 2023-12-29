add_executable(metadata_hermes metadata_hermes.cpp)
target_include_directories(metadata_hermes PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${Hermes_INCLUDE_DIRS}
        ${ADIOS2_INCLUDE_DIR})
target_link_libraries(metadata_hermes PRIVATE
        ${Hermes_LIBRARIES}
        ${ADIOS2_LIBRARIES}
        spdlog
        cereal
        rapidjson)

#add_executable(metadata_empress metadata_empress.cpp)
#target_include_directories(metadata_empress PRIVATE
#        ${PROJECT_SOURCE_DIR}/include
#        ${Hermes_INCLUDE_DIRS}
#        ${ADIOS2_INCLUDE_DIR}
#        ${SQLite3_INCLUDE_DIRS}
#        )
#target_link_libraries(metadata_empress PRIVATE
#        ${Hermes_LIBRARIES}
#        ${ADIOS2_LIBRARIES}
#        spdlog
#        cereal
#        rapidjson
#        ${SQLite3_LIBRARIES}
#        )
