cmake_minimum_required(VERSION 3.10)
project(COEUS)

set(CMAKE_CXX_STANDARD 17)

#------------------------------------------------------------------------------
# Global variables
#------------------------------------------------------------------------------
set(COEUS_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${COEUS_ROOT}/include)

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

#------------------------------------------------------------------------------
# Optimization
#------------------------------------------------------------------------------
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message("IN DEBUG MODE")
    set(CMAKE_CXX_FLAGS "-g -O0")
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    message("IN RELEASE MODE")
    set(CMAKE_CXX_FLAGS "-g -O3")
endif()

#------------------------------------------------------------------------------
# External libraries
#------------------------------------------------------------------------------

# YAML-CPP
find_package(yaml-cpp REQUIRED)
message(STATUS "found yaml-cpp at ${yaml-cpp_DIR}")

# COEUS
find_package(ADIOS2 REQUIRED)
message(STATUS "ADIOS2_FOUND: ${ADIOS2_FOUND}")
message(STATUS "ADIOS2_INCLUDE_DIRS: ${ADIOS2_INCLUDE_DIRS}")
message(STATUS "ADIOS2_LIBRARIES: ${ADIOS2_LIBRARIES}")

# Hermes
find_package(hermes REQUIRED)
message(STATUS "found hermes")

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
add_custom_target(lint COMMAND bash ${COEUS_ROOT}/scripts/lint.sh ${COEUS_ROOT})

#------------------------------------------------------------------------------
# Build tests + benchmarks
#------------------------------------------------------------------------------
set(TEST_MAIN ${COEUS_ROOT}/test/unit)
enable_testing()
add_subdirectory(test)

#------------------------------------------------------------------------------
# Install COEUS
#------------------------------------------------------------------------------
install(DIRECTORY include DESTINATION ${CMAKE_INSTALL_PREFIX})
