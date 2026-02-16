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

# #region agent log — DEBUG DIAGNOSTICS (remove after investigation)
message(STATUS "=== COEUS DEBUG DIAGNOSTICS START ===")
# H-B: Which iowarp-core installation is cmake using?
message(STATUS "[DIAG-HB] iowarp-core_DIR = ${iowarp-core_DIR}")
message(STATUS "[DIAG-HB] CMAKE_PREFIX_PATH = ${CMAKE_PREFIX_PATH}")
# H-D/E: What is chimaera::cxx? Where is the .so file?
if(TARGET chimaera::cxx)
  get_target_property(_CHI_TYPE chimaera::cxx TYPE)
  get_target_property(_CHI_LOC  chimaera::cxx IMPORTED_LOCATION)
  get_target_property(_CHI_LOC_REL chimaera::cxx IMPORTED_LOCATION_RELEASE)
  get_target_property(_CHI_LOC_NOCONFIG chimaera::cxx IMPORTED_LOCATION_NOCONFIG)
  get_target_property(_CHI_SONAME chimaera::cxx IMPORTED_SONAME)
  get_target_property(_CHI_INC  chimaera::cxx INTERFACE_INCLUDE_DIRECTORIES)
  get_target_property(_CHI_LIBS chimaera::cxx INTERFACE_LINK_LIBRARIES)
  message(STATUS "[DIAG-HDE] chimaera::cxx TYPE = ${_CHI_TYPE}")
  message(STATUS "[DIAG-HDE] chimaera::cxx IMPORTED_LOCATION = ${_CHI_LOC}")
  message(STATUS "[DIAG-HDE] chimaera::cxx IMPORTED_LOCATION_RELEASE = ${_CHI_LOC_REL}")
  message(STATUS "[DIAG-HDE] chimaera::cxx IMPORTED_LOCATION_NOCONFIG = ${_CHI_LOC_NOCONFIG}")
  message(STATUS "[DIAG-HDE] chimaera::cxx IMPORTED_SONAME = ${_CHI_SONAME}")
  message(STATUS "[DIAG-HDE] chimaera::cxx INTERFACE_INCLUDE_DIRECTORIES = ${_CHI_INC}")
  message(STATUS "[DIAG-HDE] chimaera::cxx INTERFACE_LINK_LIBRARIES = ${_CHI_LIBS}")
  # H-C/D: Check if the .so actually has PoolQuery symbols
  if(_CHI_LOC AND NOT _CHI_LOC STREQUAL "_CHI_LOC-NOTFOUND")
    execute_process(
      COMMAND nm -D "${_CHI_LOC}"
      COMMAND grep -c PoolQuery
      OUTPUT_VARIABLE _PQ_COUNT OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET)
    message(STATUS "[DIAG-HCD] nm -D on ${_CHI_LOC} | grep -c PoolQuery = ${_PQ_COUNT}")
    execute_process(
      COMMAND nm -D "${_CHI_LOC}"
      COMMAND grep -c AwakenWorker
      OUTPUT_VARIABLE _AW_COUNT OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET)
    message(STATUS "[DIAG-HCD] nm -D on ${_CHI_LOC} | grep -c AwakenWorker = ${_AW_COUNT}")
    execute_process(
      COMMAND file "${_CHI_LOC}"
      OUTPUT_VARIABLE _FILE_INFO OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET)
    message(STATUS "[DIAG-HE] file ${_CHI_LOC} = ${_FILE_INFO}")
  elseif(_CHI_LOC_REL AND NOT _CHI_LOC_REL STREQUAL "_CHI_LOC_REL-NOTFOUND")
    execute_process(
      COMMAND nm -D "${_CHI_LOC_REL}"
      COMMAND grep -c PoolQuery
      OUTPUT_VARIABLE _PQ_COUNT OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET)
    message(STATUS "[DIAG-HCD] nm -D on ${_CHI_LOC_REL} | grep -c PoolQuery = ${_PQ_COUNT}")
    execute_process(
      COMMAND nm -D "${_CHI_LOC_REL}"
      COMMAND grep -c AwakenWorker
      OUTPUT_VARIABLE _AW_COUNT OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET)
    message(STATUS "[DIAG-HCD] nm -D on ${_CHI_LOC_REL} | grep -c AwakenWorker = ${_AW_COUNT}")
    execute_process(
      COMMAND file "${_CHI_LOC_REL}"
      OUTPUT_VARIABLE _FILE_INFO OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET)
    message(STATUS "[DIAG-HE] file ${_CHI_LOC_REL} = ${_FILE_INFO}")
  elseif(_CHI_LOC_NOCONFIG AND NOT _CHI_LOC_NOCONFIG STREQUAL "_CHI_LOC_NOCONFIG-NOTFOUND")
    execute_process(
      COMMAND nm -D "${_CHI_LOC_NOCONFIG}"
      COMMAND grep -c PoolQuery
      OUTPUT_VARIABLE _PQ_COUNT OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET)
    message(STATUS "[DIAG-HCD] nm -D on ${_CHI_LOC_NOCONFIG} | grep -c PoolQuery = ${_PQ_COUNT}")
    execute_process(
      COMMAND nm -D "${_CHI_LOC_NOCONFIG}"
      COMMAND grep -c AwakenWorker
      OUTPUT_VARIABLE _AW_COUNT OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET)
    message(STATUS "[DIAG-HCD] nm -D on ${_CHI_LOC_NOCONFIG} | grep -c AwakenWorker = ${_AW_COUNT}")
  else()
    message(STATUS "[DIAG-HE] chimaera::cxx has NO IMPORTED_LOCATION set (may be INTERFACE-only)")
  endif()
  # H-D: Check if installed pool_query.h declares out-of-line methods
  if(_CHI_INC AND NOT _CHI_INC STREQUAL "_CHI_INC-NOTFOUND")
    string(REPLACE ";" "\n  " _INC_PRETTY "${_CHI_INC}")
    message(STATUS "[DIAG-HD] Include dirs:\n  ${_INC_PRETTY}")
    foreach(_inc_dir ${_CHI_INC})
      if(EXISTS "${_inc_dir}/chimaera/pool_query.h")
        message(STATUS "[DIAG-HD] Found pool_query.h at: ${_inc_dir}/chimaera/pool_query.h")
        execute_process(
          COMMAND grep -c "static PoolQuery Local" "${_inc_dir}/chimaera/pool_query.h"
          OUTPUT_VARIABLE _PQ_DECL OUTPUT_STRIP_TRAILING_WHITESPACE
          ERROR_QUIET)
        message(STATUS "[DIAG-HD] pool_query.h declares 'static PoolQuery Local' count = ${_PQ_DECL}")
        execute_process(
          COMMAND grep -c "inline" "${_inc_dir}/chimaera/pool_query.h"
          OUTPUT_VARIABLE _PQ_INLINE OUTPUT_STRIP_TRAILING_WHITESPACE
          ERROR_QUIET)
        message(STATUS "[DIAG-HD] pool_query.h 'inline' keyword count = ${_PQ_INLINE}")
      endif()
    endforeach()
  endif()
else()
  message(STATUS "[DIAG-HB] chimaera::cxx TARGET DOES NOT EXIST")
endif()
message(STATUS "=== COEUS DEBUG DIAGNOSTICS END ===")
# #endregion agent log

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
