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

# #region agent log — DEBUG DIAGNOSTICS ROUND 3 (remove after investigation)
message(STATUS "=== COEUS DEBUG DIAGNOSTICS R3 START ===")
set(_INST_DIR "/home/hxu40/spack/opt/spack/linux-skylake_avx512/iowarp-main-zrgpndlulioi4r7mdjgqycbmhrdefu43/include")
# R3-1: Show CHI_MAIN_ALLOC_T in INSTALLED types.h (should be ArenaAllocator)
execute_process(
  COMMAND grep -n "CHI_MAIN_ALLOC_T" "${_INST_DIR}/chimaera/types.h"
  OUTPUT_VARIABLE _ALLOC OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
message(STATUS "[R3-1] INSTALLED types.h CHI_MAIN_ALLOC_T:\n${_ALLOC}")
# R3-2: Show CHI_MAIN_ALLOC_T in LOCAL types.h (should be MultiProcessAllocator)
execute_process(
  COMMAND grep -n "CHI_MAIN_ALLOC_T" "${CMAKE_SOURCE_DIR}/context-runtime/include/chimaera/types.h"
  OUTPUT_VARIABLE _LOCAL_ALLOC OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
message(STATUS "[R3-2] LOCAL context-runtime types.h CHI_MAIN_ALLOC_T:\n${_LOCAL_ALLOC}")
# R3-3: Check INTERFACE_INCLUDE_DIRECTORIES of ALL linked targets
foreach(_tgt chimaera::cxx chimaera::admin_client chimaera::bdev_client wrp_cte::core_client)
  if(TARGET ${_tgt})
    get_target_property(_inc ${_tgt} INTERFACE_INCLUDE_DIRECTORIES)
    message(STATUS "[R3-3] ${_tgt} INTERFACE_INCLUDE_DIRECTORIES = ${_inc}")
  else()
    message(STATUS "[R3-3] ${_tgt} TARGET NOT FOUND")
  endif()
endforeach()
# R3-4: Check HSHM targets too (they come from context-transport-primitives)
foreach(_tgt hshm::cxx hshm::serialize hshm::lightbeam)
  if(TARGET ${_tgt})
    get_target_property(_inc ${_tgt} INTERFACE_INCLUDE_DIRECTORIES)
    message(STATUS "[R3-4] ${_tgt} INTERFACE_INCLUDE_DIRECTORIES = ${_inc}")
  else()
    message(STATUS "[R3-4] ${_tgt} TARGET NOT FOUND")
  endif()
endforeach()
# R3-5: Check if any include path contains context-runtime or context-transport
# (this would mean local headers shadow installed ones)
message(STATUS "[R3-5] CMAKE_SOURCE_DIR = ${CMAKE_SOURCE_DIR}")
message(STATUS "[R3-5] Look for local header pollution in include paths above ^^^")
message(STATUS "=== COEUS DEBUG DIAGNOSTICS R3 END ===")
message(STATUS "")
message(STATUS ">>> ALSO NEEDED: After cmake, please run:")
message(STATUS ">>>   make VERBOSE=1 2>&1 | grep 'hermes_engine.cc' | head -1")
message(STATUS ">>> and paste the output. This shows ALL compiler include flags.")
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
