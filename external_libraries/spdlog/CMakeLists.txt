MESSAGE(STATUS "SPDLOG: true")
find_package(Threads REQUIRED)
add_library(spdlog INTERFACE)
# add_library(spdlog::spdlog_header_only ALIAS ${TARGET_LOGGER})
target_include_directories(spdlog INTERFACE "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")
target_link_libraries(spdlog INTERFACE Threads::Threads)
