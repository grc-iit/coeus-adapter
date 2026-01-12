#------------------------------------------------------------------------------
# Build coeus_mdm Chimaera Module
#------------------------------------------------------------------------------

cmake_minimum_required(VERSION 3.10)

# Unified iowarp-core package - includes HermesShm, Chimaera, and all ChiMods
find_package(iowarp-core REQUIRED)

# Find SQLite3
find_package(SQLite3 REQUIRED)

# Get the include directory for coeus_mdm
set(COEUS_MDM_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)

#------------------------------------------------------------------------------
# Create client library
#------------------------------------------------------------------------------
add_library(chimaera_coeus_mdm_client SHARED
    src/coeus_mdm_client.cc
)

target_include_directories(chimaera_coeus_mdm_client PUBLIC
    ${COEUS_MDM_INCLUDE_DIR}
    ${PROJECT_SOURCE_DIR}/include  # For common headers like DbOperation.h
)

target_link_libraries(chimaera_coeus_mdm_client
    chimaera::cxx
    chimaera::admin_client
)

# Enable C++20 coroutines for client (required for Chimaera headers)
set_target_properties(chimaera_coeus_mdm_client PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)
# Add coroutines flag for GCC/Clang
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(chimaera_coeus_mdm_client PRIVATE -fcoroutines)
endif()

#------------------------------------------------------------------------------
# Create runtime library
#------------------------------------------------------------------------------
add_library(chimaera_coeus_mdm_runtime SHARED
    src/coeus_mdm_runtime.cc
    src/autogen/coeus_mdm_lib_exec.cc
)

target_include_directories(chimaera_coeus_mdm_runtime PUBLIC
    ${COEUS_MDM_INCLUDE_DIR}
    ${SQLite3_INCLUDE_DIRS}
    ${PROJECT_SOURCE_DIR}/include  # For common headers like DbOperation.h, SQlite.h
)

# Link SQLite3 - use imported target if available, otherwise use variables
if(TARGET SQLite3::SQLite3)
    target_link_libraries(chimaera_coeus_mdm_runtime
        chimaera::cxx
        chimaera::admin_client
        SQLite3::SQLite3
    )
else()
    # Fallback to variables if imported target not available
    target_link_libraries(chimaera_coeus_mdm_runtime
        chimaera::cxx
        chimaera::admin_client
        ${SQLite3_LIBRARIES}
    )
endif()

# Enable C++20 coroutines for runtime (required for TaskResume return type)
set_target_properties(chimaera_coeus_mdm_runtime PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)
# Add coroutines flag for GCC/Clang
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(chimaera_coeus_mdm_runtime PRIVATE -fcoroutines)
endif()

#------------------------------------------------------------------------------
# Install libraries and headers
#------------------------------------------------------------------------------
install(
    TARGETS
    chimaera_coeus_mdm_client
    chimaera_coeus_mdm_runtime
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
    DESTINATION ${CMAKE_INSTALL_PREFIX}/share/chimaera/modules/coeus_mdm
)
