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

# #region agent log — DEBUG DIAGNOSTICS ROUND 2 (remove after investigation)
message(STATUS "=== COEUS DEBUG DIAGNOSTICS R2 START ===")
set(_CHI_SO "/home/hxu40/spack/opt/spack/linux-skylake_avx512/iowarp-main-zrgpndlulioi4r7mdjgqycbmhrdefu43/lib/libchimaera_cxx.so.1.0.0")
set(_CHI_HDR "/home/hxu40/spack/opt/spack/linux-skylake_avx512/iowarp-main-zrgpndlulioi4r7mdjgqycbmhrdefu43/include/chimaera/pool_query.h")
set(_CHI_IPC_HDR "/home/hxu40/spack/opt/spack/linux-skylake_avx512/iowarp-main-zrgpndlulioi4r7mdjgqycbmhrdefu43/include/chimaera/ipc_manager.h")
# H-F: Show DEMANGLED PoolQuery symbols in the .so to see exact signatures
execute_process(
  COMMAND nm -D --demangle "${_CHI_SO}"
  COMMAND grep "PoolQuery"
  OUTPUT_VARIABLE _PQ_SYMS OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_QUIET)
message(STATUS "[DIAG-HF] Demangled PoolQuery symbols in .so:\n${_PQ_SYMS}")
# H-F: Show DEMANGLED AwakenWorker symbols
execute_process(
  COMMAND nm -D --demangle "${_CHI_SO}"
  COMMAND grep "AwakenWorker"
  OUTPUT_VARIABLE _AW_SYMS OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_QUIET)
message(STATUS "[DIAG-HF] Demangled AwakenWorker symbols in .so:\n${_AW_SYMS}")
# H-F/G: Show the INSTALLED pool_query.h content (first 60 lines)
execute_process(
  COMMAND head -n 80 "${_CHI_HDR}"
  OUTPUT_VARIABLE _PQ_CONTENT OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_QUIET)
message(STATUS "[DIAG-HFG] Installed pool_query.h (first 80 lines):\n${_PQ_CONTENT}")
# H-G: Check if there's a PoolQuery class at all in the installed header
execute_process(
  COMMAND grep -n "PoolQuery" "${_CHI_HDR}"
  OUTPUT_VARIABLE _PQ_GREP OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_QUIET)
message(STATUS "[DIAG-HG] grep PoolQuery in installed pool_query.h:\n${_PQ_GREP}")
# H-G: Check the ipc_manager.h for AwakenWorker declaration
execute_process(
  COMMAND grep -n "AwakenWorker" "${_CHI_IPC_HDR}"
  OUTPUT_VARIABLE _AW_GREP OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_QUIET)
message(STATUS "[DIAG-HG] grep AwakenWorker in installed ipc_manager.h:\n${_AW_GREP}")
# H-H: Check what pool_query.h the COMPILER actually resolves
# (is it the installed one, or a local one from context-runtime?)
execute_process(
  COMMAND find "${CMAKE_SOURCE_DIR}" -name "pool_query.h" -not -path "*/build/*"
  OUTPUT_VARIABLE _LOCAL_PQ OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_QUIET)
message(STATUS "[DIAG-HH] Local pool_query.h files in source tree:\n${_LOCAL_PQ}")
# H-H: Show all include paths that hermes_engine will use
get_target_property(_HE_INCS hermes_engine INCLUDE_DIRECTORIES)
message(STATUS "[DIAG-HH] hermes_engine INCLUDE_DIRECTORIES = ${_HE_INCS}")
get_target_property(_HE_IFACE_INCS hermes_engine INTERFACE_INCLUDE_DIRECTORIES)
message(STATUS "[DIAG-HH] hermes_engine INTERFACE_INCLUDE_DIRECTORIES = ${_HE_IFACE_INCS}")
message(STATUS "=== COEUS DEBUG DIAGNOSTICS R2 END ===")
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
