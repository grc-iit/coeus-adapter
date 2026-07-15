# Time-derivative operator: pure C++, no extra third-party deps.
add_executable(coeus_tderiv
    ${CMAKE_CURRENT_SOURCE_DIR}/TimeDerivativeOperator.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/tderiv_main.cc)

target_include_directories(coeus_tderiv PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(coeus_tderiv PRIVATE coeus_ops_common)
set_target_properties(coeus_tderiv PROPERTIES
    CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
target_compile_options(coeus_tderiv PRIVATE -fcoroutines)

install(TARGETS coeus_tderiv RUNTIME DESTINATION ${COEUS_INSTALL_BIN_DIR})
