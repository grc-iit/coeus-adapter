#------------------------------------------------------------------------------
# Build rankConsensus Chimaera Module
#------------------------------------------------------------------------------

cmake_minimum_required(VERSION 3.10)

# Unified iowarp-core package - includes HermesShm, Chimaera, and all ChiMods
find_package(iowarp-core REQUIRED)

# Get the include directory for rankConsensus
set(RANKCONSENSUS_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)

#------------------------------------------------------------------------------
# Create client library
#------------------------------------------------------------------------------
add_library(chimaera_rankConsensus_client SHARED
    src/rankConsensus_client.cc
)

target_include_directories(chimaera_rankConsensus_client PUBLIC
    ${RANKCONSENSUS_INCLUDE_DIR}
    ${PROJECT_SOURCE_DIR}/include  # For common headers
)

target_link_libraries(chimaera_rankConsensus_client
    chimaera::cxx
    chimaera::admin_client
)

# Enable C++20 coroutines for client (required for Chimaera headers)
set_target_properties(chimaera_rankConsensus_client PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)
# Add coroutines flag for GCC/Clang
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(chimaera_rankConsensus_client PRIVATE -fcoroutines)
endif()

#------------------------------------------------------------------------------
# Create runtime library
#------------------------------------------------------------------------------
add_library(chimaera_rankConsensus_runtime SHARED
    src/rankConsensus_runtime.cc
    src/autogen/rankConsensus_lib_exec.cc
)

target_include_directories(chimaera_rankConsensus_runtime PUBLIC
    ${RANKCONSENSUS_INCLUDE_DIR}
    ${PROJECT_SOURCE_DIR}/include  # For common headers
)

target_link_libraries(chimaera_rankConsensus_runtime
    chimaera::cxx
    chimaera::admin_client
)

# Enable C++20 coroutines for runtime (required for TaskResume return type)
set_target_properties(chimaera_rankConsensus_runtime PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)
# Add coroutines flag for GCC/Clang
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(chimaera_rankConsensus_runtime PRIVATE -fcoroutines)
endif()

#------------------------------------------------------------------------------
# Install libraries and headers
#------------------------------------------------------------------------------
install(
    TARGETS
    chimaera_rankConsensus_client
    chimaera_rankConsensus_runtime
    EXPORT
    ${COEUS_EXPORTED_TARGETS}
    LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
    ARCHIVE DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
    RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
)

install(
    DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_PREFIX}/include
    FILES_MATCHING
    PATTERN "*.h"
)

# Install chimaera_mod.yaml
install(
    FILES chimaera_mod.yaml
    DESTINATION ${CMAKE_INSTALL_PREFIX}/share/chimaera/modules/rankConsensus
)
