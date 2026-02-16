include_directories(${CMAKE_SOURCE_DIR})

#------------------------------------------------------------------------------
# Set sources
#------------------------------------------------------------------------------
set(HERMES_ENGINE_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/hermes_engine.cc
        ${CMAKE_SOURCE_DIR}/include/comms/CTETagClient.cc
        ${CMAKE_SOURCE_DIR}/include/comms/CTEHermes.cc)

#------------------------------------------------------------------------------
# Libraries
#------------------------------------------------------------------------------
add_library(hermes_engine ${HERMES_ENGINE_SRCS} ../include/coeus/MetadataSerializer.h)
add_dependencies(hermes_engine spdlog)
add_dependencies(hermes_engine chimaera_coeus_mdm_client)
add_dependencies(hermes_engine chimaera_coeus_mdm_runtime)
add_dependencies(hermes_engine chimaera_rankConsensus_client)
add_dependencies(hermes_engine chimaera_rankConsensus_runtime)
target_include_directories(hermes_engine PUBLIC
        ../include
        ${ADIOS2_INCLUDE_DIR}
        ${SQLite3_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/tasks/coeus_mdm/include
        ${CMAKE_SOURCE_DIR}/tasks/rankConsensus/include
        )
# #region agent log — DEBUG DIAGNOSTICS Round 2
set(_DBG_LOG "${CMAKE_SOURCE_DIR}/.cursor/debug.log")
file(WRITE ${_DBG_LOG} "")
get_target_property(_CXX_LOC_REL chimaera::cxx IMPORTED_LOCATION_RELEASE)
get_target_property(_CXX_INC_DIRS chimaera::cxx INTERFACE_INCLUDE_DIRECTORIES)
# (B2) Check if installed libchimaera_cxx.so contains the missing symbols
if(_CXX_LOC_REL AND NOT _CXX_LOC_REL STREQUAL "_CXX_LOC_REL-NOTFOUND")
  execute_process(
    COMMAND nm -DC "${_CXX_LOC_REL}"
    OUTPUT_VARIABLE _NM_OUTPUT ERROR_VARIABLE _NM_ERR RESULT_VARIABLE _NM_RES)
  # Search for PoolQuery symbols
  string(FIND "${_NM_OUTPUT}" "PoolQuery" _PQ_POS)
  # Search for AwakenWorker
  string(FIND "${_NM_OUTPUT}" "AwakenWorker" _AW_POS)
  file(APPEND ${_DBG_LOG} "{\"hypothesisId\":\"B2\",\"message\":\"nm on libchimaera_cxx\",\"data\":{\"pool_query_found_pos\":\"${_PQ_POS}\",\"awaken_worker_found_pos\":\"${_AW_POS}\",\"nm_res\":\"${_NM_RES}\"}}\n")
  message(STATUS "[DEBUG] nm result: PoolQuery at pos ${_PQ_POS}, AwakenWorker at pos ${_AW_POS}")
  # Dump the actual matching lines
  string(REPLACE "\n" ";" _NM_LINES "${_NM_OUTPUT}")
  foreach(_line ${_NM_LINES})
    string(FIND "${_line}" "PoolQuery" _pos1)
    string(FIND "${_line}" "AwakenWorker" _pos2)
    if(NOT _pos1 EQUAL -1 OR NOT _pos2 EQUAL -1)
      message(STATUS "[DEBUG-NM] ${_line}")
      file(APPEND ${_DBG_LOG} "{\"hypothesisId\":\"B2\",\"message\":\"nm line\",\"data\":{\"line\":\"${_line}\"}}\n")
    endif()
  endforeach()
endif()
# (C2) Check if installed pool_query.h has inline definitions
list(GET _CXX_INC_DIRS 0 _FIRST_INC_DIR)
set(_INSTALLED_PQH "${_FIRST_INC_DIR}/chimaera/pool_query.h")
if(EXISTS "${_INSTALLED_PQH}")
  file(READ "${_INSTALLED_PQH}" _PQH_CONTENT)
  string(FIND "${_PQH_CONTENT}" "HSHM_CROSS_FUN PoolQuery()" _PQ_INLINE_POS)
  string(FIND "${_PQH_CONTENT}" "static HSHM_CROSS_FUN PoolQuery Local()" _LOCAL_INLINE_POS)
  string(LENGTH "${_PQH_CONTENT}" _PQH_LEN)
  file(APPEND ${_DBG_LOG} "{\"hypothesisId\":\"C2\",\"message\":\"installed pool_query.h\",\"data\":{\"exists\":true,\"length\":${_PQH_LEN},\"has_inline_ctor_pos\":\"${_PQ_INLINE_POS}\",\"has_inline_local_pos\":\"${_LOCAL_INLINE_POS}\"}}\n")
  message(STATUS "[DEBUG] installed pool_query.h exists, len=${_PQH_LEN}, inline_ctor_pos=${_PQ_INLINE_POS}, inline_Local_pos=${_LOCAL_INLINE_POS}")
else()
  file(APPEND ${_DBG_LOG} "{\"hypothesisId\":\"C2\",\"message\":\"installed pool_query.h\",\"data\":{\"exists\":false,\"path\":\"${_INSTALLED_PQH}\"}}\n")
  message(WARNING "[DEBUG] installed pool_query.h NOT FOUND at ${_INSTALLED_PQH}")
endif()
# (C3) Check installed types.h for CHI_MAIN_ALLOC_T (ABI mismatch root cause)
set(_INSTALLED_TYPES "${_FIRST_INC_DIR}/chimaera/types.h")
if(EXISTS "${_INSTALLED_TYPES}")
  file(READ "${_INSTALLED_TYPES}" _TYPES_CONTENT)
  string(FIND "${_TYPES_CONTENT}" "CHI_MAIN_ALLOC_T" _ALLOC_POS)
  # Extract the line containing CHI_MAIN_ALLOC_T
  string(REGEX MATCH "#define CHI_MAIN_ALLOC_T [^\n]+" _ALLOC_LINE "${_TYPES_CONTENT}")
  string(REGEX MATCH "#define CHI_CDATA_ALLOC_T [^\n]+" _CDATA_LINE "${_TYPES_CONTENT}")
  file(APPEND ${_DBG_LOG} "{\"hypothesisId\":\"ABI\",\"message\":\"installed types.h alloc\",\"data\":{\"main_alloc\":\"${_ALLOC_LINE}\",\"cdata_alloc\":\"${_CDATA_LINE}\"}}\n")
  message(STATUS "[DEBUG] installed types.h CHI_MAIN_ALLOC_T: ${_ALLOC_LINE}")
  message(STATUS "[DEBUG] installed types.h CHI_CDATA_ALLOC_T: ${_CDATA_LINE}")
else()
  message(WARNING "[DEBUG] installed types.h NOT FOUND at ${_INSTALLED_TYPES}")
endif()
# (C4) Check installed pool_query.h - is Local() an inline DEFINITION or just a declaration?
if(EXISTS "${_INSTALLED_PQH}")
  file(READ "${_INSTALLED_PQH}" _PQH_RAW)
  string(REGEX MATCH "Local\\(\\)[^\n]*" _LOCAL_LINE "${_PQH_RAW}")
  file(APPEND ${_DBG_LOG} "{\"hypothesisId\":\"C4\",\"message\":\"Local() line\",\"data\":{\"line\":\"${_LOCAL_LINE}\"}}\n")
  message(STATUS "[DEBUG] installed pool_query.h Local() line: ${_LOCAL_LINE}")
endif()
# #endregion agent log
target_link_libraries(hermes_engine LINK_PUBLIC
        ${ADIOS2_LIBRARIES}
        spdlog
        cereal
        rapidjson
        ${SQLite3_LIBRARIES}
        # Unified iowarp-core targets (provided by iowarp-core package)
        chimaera::cxx              # Core Chimaera library
        chimaera::admin_client    # Admin ChiMod (always available)
        chimaera::bdev_client     # Block device ChiMod (always available)
        wrp_cte::core_client      # CTE client for I/O placement
        # Local ChiMods
        chimaera_coeus_mdm_client
        chimaera_coeus_mdm_runtime
        chimaera_rankConsensus_client
        chimaera_rankConsensus_runtime)


# Enable C++20 coroutines for hermes_engine (needed for Chimaera TaskResume)
set_target_properties(hermes_engine PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON)
target_compile_options(hermes_engine PRIVATE -fcoroutines)
#############################################
# Optional Catalyst/Fides in-situ integration
#############################################
# Try to locate Catalyst (Catalyst 2 provides a CMake target catalyst::catalyst)
find_package(Catalyst QUIET CONFIG)
# Enable COEUS_HAVE_CATALYST if Catalyst was found OR user set COEUS_ENABLE_CATALYST=ON
if(Catalyst_FOUND OR COEUS_ENABLE_CATALYST)
  target_compile_definitions(hermes_engine PRIVATE COEUS_HAVE_CATALYST=1)
  if(Catalyst_FOUND)
    message(STATUS "Catalyst found: enabling in-situ integration in hermes_engine")
    target_link_libraries(hermes_engine PRIVATE catalyst::catalyst)
  else()
    message(STATUS "COEUS_ENABLE_CATALYST=ON: COEUS_HAVE_CATALYST defined but Catalyst not found (link at runtime or disable)")
  endif()
else()
  message(STATUS "Catalyst not found and COEUS_ENABLE_CATALYST=OFF: building without in-situ integration")
endif()
#-----------------------------------------------------------------------------
# Add Target(s) to CMake Install
#-----------------------------------------------------------------------------
install(
  TARGETS
    hermes_engine
  EXPORT
    ${COEUS_EXPORTED_TARGETS}
  LIBRARY DESTINATION ${COEUS_INSTALL_LIB_DIR}
  ARCHIVE DESTINATION ${COEUS_INSTALL_LIB_DIR}
  RUNTIME DESTINATION ${COEUS_INSTALL_BIN_DIR}
)
#
##-----------------------------------------------------------------------------
## Add Target(s) to CMake Install for import into other projects
##-----------------------------------------------------------------------------
#install(
#  EXPORT
#    ${COEUS_EXPORTED_TARGETS}
#  DESTINATION
#    ${COEUS_INSTALL_DATA_DIR}/cmake/COEUS
#  FILE
#    ${COEUS_EXPORTED_TARGETS}.cmake
#)

#-----------------------------------------------------------------------------
# Export all exported targets to the build tree for use by parent project
#-----------------------------------------------------------------------------
if(NOT COEUS_EXTERNALLY_CONFIGURED)
EXPORT (
  TARGETS
    ${COEUS_EXPORTED_LIBS}
  FILE
    ${COEUS_EXPORTED_TARGETS}.cmake
)
endif()

#------------------------------------------------------------------------------
# Set variables for parent scope
#------------------------------------------------------------------------------
# Used by config.cmake.build.in and Testing
set(COEUS_INCLUDES_BUILD_TIME
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
  ${COEUS_EXT_INCLUDE_DEPENDENCIES}
  PARENT_SCOPE)

# Used by config.cmake.install.in
set(COEUS_INCLUDES_INSTALL_TIME
  ${COEUS_INSTALL_INCLUDE_DIR}
  ${COEUS_EXT_INCLUDE_DEPENDENCIES}
  PARENT_SCOPE)
