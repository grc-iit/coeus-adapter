#------------------------------------------------------------------------------
# Build coeus_mdm ChiMod (clio-core)
#------------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.20)

find_package(SQLite3 REQUIRED)

# Coeus root include (for common/DbOperation.h, common/SQlite.h). Computed
# relative to this module so it works whether built standalone or as part of
# the full coeus-adapter tree.
get_filename_component(COEUS_INCLUDE_ROOT
  "${CMAKE_CURRENT_SOURCE_DIR}/../../include" ABSOLUTE)

# Client library -> target coeus_coeus_mdm_client, alias coeus::coeus_mdm_client
add_clio_module_client(
  SOURCES
    src/coeus_mdm_client.cc
  INCLUDE_DIRECTORIES
    ${COEUS_INCLUDE_ROOT}
)

# Runtime library -> target coeus_coeus_mdm_runtime, alias coeus::coeus_mdm_runtime
add_clio_module_runtime(
  SOURCES
    src/coeus_mdm_runtime.cc
    src/autogen/coeus_mdm_lib_exec.cc
  LINK_LIBRARIES
    ${SQLite3_LIBRARIES}
  INCLUDE_DIRECTORIES
    ${COEUS_INCLUDE_ROOT}
    ${SQLite3_INCLUDE_DIRS}
)
