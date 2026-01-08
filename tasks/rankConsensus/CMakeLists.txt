#------------------------------------------------------------------------------
# Build rankConsensus Chimaera Module
#------------------------------------------------------------------------------

cmake_minimum_required(VERSION 3.10)

# Find Chimaera packages
find_package(chimaera-core REQUIRED)
find_package(chimaera-admin REQUIRED)

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
    ${chimaera-core_INCLUDE_DIRS}
    ${chimaera-admin_INCLUDE_DIRS}
    ${PROJECT_SOURCE_DIR}/include  # For common headers
)

target_link_libraries(chimaera_rankConsensus_client
    chimaera::cxx
    chimaera::admin_client
)

#------------------------------------------------------------------------------
# Create runtime library
#------------------------------------------------------------------------------
add_library(chimaera_rankConsensus_runtime SHARED
    src/rankConsensus_runtime.cc
    src/autogen/rankConsensus_lib_exec.cc
)

target_include_directories(chimaera_rankConsensus_runtime PUBLIC
    ${RANKCONSENSUS_INCLUDE_DIR}
    ${chimaera-core_INCLUDE_DIRS}
    ${chimaera-admin_INCLUDE_DIRS}
    ${PROJECT_SOURCE_DIR}/include  # For common headers
)

target_link_libraries(chimaera_rankConsensus_runtime
    chimaera::cxx
    chimaera::admin_client
)

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
