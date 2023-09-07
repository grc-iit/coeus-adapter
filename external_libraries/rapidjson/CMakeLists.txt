MESSAGE(STATUS "rapidjson: true")
add_library(rapidjson INTERFACE)
target_include_directories(rapidjson INTERFACE include)