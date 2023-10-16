cmake_minimum_required(VERSION 3.10)
project(COEUS)

set(CMAKE_CXX_STANDARD 17)

#------------------------------------------------------------------------------
# Global variable
#------------------------------------------------------------------------------
set(COEUS_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
MESSAGE(STATUS "CMAKE_PREFIX_PATH: $ENV{CMAKE_PREFIX_PATH}")
#------------------------------------------------------------------------------
# Solving some weirdness with CLion docker toolchain
#------------------------------------------------------------------------------
if(EXISTS "/.dockerenv")
    # Inside a Docker container
    file(GLOB ADIOS2_INCLUDE_DIR "/opt/software/linux-ubuntu22.04-zen2/gcc-11.3.0/adios2-*/include")
    file(GLOB Hermes_INCLUDE_DIRS "/opt/software/linux-ubuntu22.04-zen2/gcc-11.3.0/hermes-master-*/include")
else()
    # Not inside a Docker container
    set(ADIOS2_INCLUDE_DIR "")
endif()
MESSAGE(STATUS "ADIOS2_INCLUDE_DIR: " ${ADIOS2_INCLUDE_DIR})
MESSAGE(STATUS "Hermes_INCLUDE_DIRS: " ${Hermes_INCLUDE_DIRS})
MESSAGE(STATUS "Hermes_INCLUDE_DIR: " ${Hermes_INCLUDE_DIR})

#-----------------------------------------------------------------------------
# Targets built within this project are exported at Install time for use
# by other projects.
#-----------------------------------------------------------------------------
if(NOT COEUS_EXPORTED_TARGETS)
    set(COEUS_EXPORTED_TARGETS "coeus-targets")
endif()

#------------------------------------------------------------------------------
# Options
#------------------------------------------------------------------------------
option(BUILD_SHARED_LIBS "Build shared libraries (.dll/.so) instead of static ones (.lib/.a)" ON)
option(COEUS_ENABLE_COVERAGE "Enable coverage calaculation" OFF)
option(COEUS_ENABLE_DOXYGEN "Enable documentation" OFF)

#------------------------------------------------------------------------------
# Setup CMake Environment
#------------------------------------------------------------------------------
if(NOT COEUS_EXTERNALLY_CONFIGURED)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY
            ${PROJECT_BINARY_DIR}/bin CACHE PATH "Single Directory for all Executables.")
    set(EXECUTABLE_OUTPUT_PATH ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY
            ${PROJECT_BINARY_DIR}/bin CACHE PATH "Single Directory for all Libraries")
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY
            ${PROJECT_BINARY_DIR}/bin CACHE PATH "Single Directory for all static libraries.")
endif()

#------------------------------------------------------------------------------
# Setup install and output Directories
#------------------------------------------------------------------------------
if(NOT COEUS_INSTALL_BIN_DIR)
    set(COEUS_INSTALL_BIN_DIR ${CMAKE_INSTALL_PREFIX}/bin)
endif()
if(NOT COEUS_INSTALL_LIB_DIR)
    set(COEUS_INSTALL_LIB_DIR ${CMAKE_INSTALL_PREFIX}/lib)
endif()
if(NOT COEUS_INSTALL_INCLUDE_DIR)
    set(COEUS_INSTALL_INCLUDE_DIR ${CMAKE_INSTALL_PREFIX}/include)
endif()
if(NOT COEUS_INSTALL_DATA_DIR)
    set(COEUS_INSTALL_DATA_DIR ${CMAKE_INSTALL_PREFIX}/share)
endif()
message(STATUS "Installing in: ${COEUS_INSTALL_BIN_DIR}")

#------------------------------------------------------------------------------
# Optimization
#------------------------------------------------------------------------------
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(WARNING "Building in debug mode")
    set(CMAKE_CXX_FLAGS "-g -O0 ${CMAKE_CXX_FLAGS}")
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    message(STATUS "Building in release mode")
    set(CMAKE_CXX_FLAGS "-g -O0 ${CMAKE_CXX_FLAGS}")
endif()

#------------------------------------------------------------------------------
# External libraries
#------------------------------------------------------------------------------
# Hermes
find_package(Hermes CONFIG REQUIRED)
message(STATUS "found Hermes at ${Hermes_DIR}")
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_definitions(HERMES_LOG_VERBOSITY=1)
endif()
include_directories(${HermesShm_INCLUDE_DIRS})
# COEUS
find_package(ADIOS2 REQUIRED)
message(STATUS "Found Adios2")
MESSAGE(STATUS "Adios config: ${ADIOS2_CONFIG}")
# MPI
find_package(MPI REQUIRED COMPONENTS C CXX)

#Yaml-cpp
find_package(yaml-cpp REQUIRED)
if(yaml-cpp_FOUND)
    message(STATUS "found yaml-cpp at ${yaml-cpp_DIR}")
endif()

# OpenMP
find_package(OpenMP REQUIRED COMPONENTS C CXX)
message(STATUS "Found OpenMP")

find_package(SQLite3 REQUIRED)
message(STATUS "Found SQLite3")
include_directories(${SQLite3_INCLUDE_DIRS})

#spdlog
add_subdirectory(external_libraries/spdlog)
add_subdirectory(external_libraries/cereal)
add_subdirectory(external_libraries/rapidjson)

#Gtest
include(FetchContent)
FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        f8d7d77c06936315286eb55f8de22cd23c188571
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)
#------------------------------------------------------------------------------
# Code Coverage
#------------------------------------------------------------------------------
if(COEUS_ENABLE_COVERAGE)
    set(COVERAGE_FLAGS "-fprofile-arcs -ftest-coverage" CACHE STRING
            "Flags to the coverage program to perform coverage inspection")
    mark_as_advanced(COVERAGE_FLAGS)

    macro(set_coverage_flags target)
        set_target_properties(${target}
                PROPERTIES
                COMPILE_FLAGS ${COVERAGE_FLAGS}
                LINK_FLAGS ${COVERAGE_FLAGS}
                )
    endmacro()
endif()

#------------------------------------------------------------------------------
# Enable Testing
#------------------------------------------------------------------------------
function(pytest test_type test_name)
    set(script ${CMAKE_SOURCE_DIR}/CI/py_coeus_ci/bin/run_test.py)

    add_test(NAME ${test_name}
            COMMAND python3 ${script} ${test_type} ${test_name} ${CMAKE_BINARY_DIR})
endfunction()

function(pytest_benchmark test_type test_name label)
    set(script ${CMAKE_SOURCE_DIR}/CI/py_coeus_ci/bin/run_bench_test.py)
    # Number of processes we want to run
    set(processes_list 1 2 4 8 16 32)
    foreach(num_processes ${processes_list})
        add_test(NAME ${test_name}_${num_processes}
                COMMAND python3 ${script} ${test_type} ${test_name} ${CMAKE_BINARY_DIR} ${num_processes})
        set_tests_properties(${test_name}_${num_processes} PROPERTIES LABELS ${label})
    endforeach ()
endfunction()



#------------------------------------------------------------------------------
# Documentation
#------------------------------------------------------------------------------
if(COEUS_ENABLE_DOXYGEN)
    include(UseDoxygenDoc)
    add_doxygen_doc(
            BUILD_DIR
            ${CMAKE_CURRENT_BINARY_DIR}/_build
            DOXY_FILE
            ${CMAKE_CURRENT_SOURCE_DIR}/doc/Doxyfile.in
            TARGET_NAME
            dox
            COMMENT
            "HTML documentation"
    )
endif()

#------------------------------------------------------------------------------
# Build COEUS
#------------------------------------------------------------------------------
add_subdirectory(src)
add_subdirectory(tasks)
#add_custom_target(lint ALL COMMAND bash ${COEUS_ROOT}/CI/lint.sh ${COEUS_ROOT})

#------------------------------------------------------------------------------
# Build tests + benchmarks
#------------------------------------------------------------------------------
set(TEST_MAIN ${PROJECT_SOURCE_DIR}/test/unit)
#set(TEST_MAIN ${CMAKE_SOURCE_DIR}/test/unit)
enable_testing()
add_subdirectory(test)

#------------------------------------------------------------------------------
# Install COEUS
#------------------------------------------------------------------------------
#install(DIRECTORY include DESTINATION ${CMAKE_INSTALL_PREFIX})