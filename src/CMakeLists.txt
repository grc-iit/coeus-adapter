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

# #region agent log — DEBUG DIAGNOSTICS ROUND 4 (remove after investigation)
message(STATUS "=== COEUS DEBUG DIAGNOSTICS R4 START ===")
set(_IOWARP_CMAKE_DIR "/home/hxu40/spack/opt/spack/linux-skylake_avx512/iowarp-main-zrgpndlulioi4r7mdjgqycbmhrdefu43/lib/cmake/iowarp-core")
# R4-1: List ALL files in the iowarp-core cmake config dir
execute_process(
  COMMAND ls -la "${_IOWARP_CMAKE_DIR}"
  OUTPUT_VARIABLE _LS OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
message(STATUS "[R4-1] iowarp-core cmake config files:\n${_LS}")
# R4-2: Show the main iowarp-coreConfig.cmake
execute_process(
  COMMAND cat "${_IOWARP_CMAKE_DIR}/iowarp-coreConfig.cmake"
  OUTPUT_VARIABLE _CFG OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
message(STATUS "[R4-2] iowarp-coreConfig.cmake:\n${_CFG}")
# R4-3: Show IowarpCoreCommon.cmake (if it exists)
execute_process(
  COMMAND cat "${_IOWARP_CMAKE_DIR}/IowarpCoreCommon.cmake"
  OUTPUT_VARIABLE _COMMON OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
message(STATUS "[R4-3] IowarpCoreCommon.cmake:\n${_COMMON}")
# R4-4: Key summary - the version mismatch
message(STATUS "[R4-4] INSTALLED CHI_MAIN_ALLOC_T = hipc::ArenaAllocator<false>")
message(STATUS "[R4-4] LOCAL     CHI_MAIN_ALLOC_T = hipc::MultiProcessAllocator")
message(STATUS "[R4-4] These are INCOMPATIBLE - this is the root cause")
message(STATUS "=== COEUS DEBUG DIAGNOSTICS R4 END ===")
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
