add_executable(basic ${CMAKE_CURRENT_SOURCE_DIR}/basic.cc)
target_link_libraries(basic coeus)
add_dependencies(basic coeus)
