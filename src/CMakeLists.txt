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
# #region agent log — DEBUG DIAGNOSTICS (Hypothesis A,B,C,D,E)
set(_DBG_LOG "${CMAKE_SOURCE_DIR}/.cursor/debug.log")
file(WRITE ${_DBG_LOG} "")
# Hypothesis A/B: Check chimaera::cxx target type and library location
if(TARGET chimaera::cxx)
  get_target_property(_CXX_TYPE chimaera::cxx TYPE)
  get_target_property(_CXX_LOC  chimaera::cxx IMPORTED_LOCATION)
  get_target_property(_CXX_LOC_REL chimaera::cxx IMPORTED_LOCATION_RELEASE)
  get_target_property(_CXX_LOC_NOCONFIG chimaera::cxx IMPORTED_LOCATION_NOCONFIG)
  get_target_property(_CXX_LINK_LIBS chimaera::cxx INTERFACE_LINK_LIBRARIES)
  get_target_property(_CXX_INC_DIRS chimaera::cxx INTERFACE_INCLUDE_DIRECTORIES)
  get_target_property(_CXX_SONAME chimaera::cxx IMPORTED_SONAME)
  file(APPEND ${_DBG_LOG} "{\"hypothesisId\":\"A\",\"message\":\"chimaera::cxx target info\",\"data\":{\"type\":\"${_CXX_TYPE}\",\"location\":\"${_CXX_LOC}\",\"location_release\":\"${_CXX_LOC_REL}\",\"location_noconfig\":\"${_CXX_LOC_NOCONFIG}\",\"soname\":\"${_CXX_SONAME}\",\"link_libs\":\"${_CXX_LINK_LIBS}\",\"include_dirs\":\"${_CXX_INC_DIRS}\"}}\n")
  message(STATUS "[DEBUG] chimaera::cxx TYPE=${_CXX_TYPE}")
  message(STATUS "[DEBUG] chimaera::cxx IMPORTED_LOCATION=${_CXX_LOC}")
  message(STATUS "[DEBUG] chimaera::cxx IMPORTED_LOCATION_RELEASE=${_CXX_LOC_REL}")
  message(STATUS "[DEBUG] chimaera::cxx IMPORTED_LOCATION_NOCONFIG=${_CXX_LOC_NOCONFIG}")
  message(STATUS "[DEBUG] chimaera::cxx INTERFACE_INCLUDE_DIRECTORIES=${_CXX_INC_DIRS}")
  message(STATUS "[DEBUG] chimaera::cxx INTERFACE_LINK_LIBRARIES=${_CXX_LINK_LIBS}")
  # Hypothesis B: Check if the library file actually exists
  if(_CXX_LOC AND NOT _CXX_LOC STREQUAL "_CXX_LOC-NOTFOUND")
    if(EXISTS "${_CXX_LOC}")
      file(APPEND ${_DBG_LOG} "{\"hypothesisId\":\"B\",\"message\":\"library file EXISTS\",\"data\":{\"path\":\"${_CXX_LOC}\"}}\n")
      message(STATUS "[DEBUG] chimaera::cxx library file EXISTS at ${_CXX_LOC}")
    else()
      file(APPEND ${_DBG_LOG} "{\"hypothesisId\":\"B\",\"message\":\"library file MISSING\",\"data\":{\"path\":\"${_CXX_LOC}\"}}\n")
      message(WARNING "[DEBUG] chimaera::cxx library file MISSING at ${_CXX_LOC}")
    endif()
  endif()
else()
  file(APPEND ${_DBG_LOG} "{\"hypothesisId\":\"A\",\"message\":\"chimaera::cxx TARGET DOES NOT EXIST\"}\n")
  message(WARNING "[DEBUG] chimaera::cxx TARGET DOES NOT EXIST!")
endif()
# Hypothesis C: Check include directories for pool_query.h
get_target_property(_HE_INC_DIRS hermes_engine INCLUDE_DIRECTORIES)
file(APPEND ${_DBG_LOG} "{\"hypothesisId\":\"C\",\"message\":\"hermes_engine include dirs\",\"data\":{\"dirs\":\"${_HE_INC_DIRS}\"}}\n")
message(STATUS "[DEBUG] hermes_engine INCLUDE_DIRECTORIES=${_HE_INC_DIRS}")
# Hypothesis E: Check other chi targets
foreach(_tgt chimaera::admin_client chimaera::bdev_client wrp_cte::core_client)
  if(TARGET ${_tgt})
    get_target_property(_T_TYPE ${_tgt} TYPE)
    get_target_property(_T_LOC ${_tgt} IMPORTED_LOCATION)
    file(APPEND ${_DBG_LOG} "{\"hypothesisId\":\"E\",\"message\":\"target ${_tgt}\",\"data\":{\"type\":\"${_T_TYPE}\",\"location\":\"${_T_LOC}\"}}\n")
    message(STATUS "[DEBUG] ${_tgt} TYPE=${_T_TYPE} LOC=${_T_LOC}")
  else()
    file(APPEND ${_DBG_LOG} "{\"hypothesisId\":\"E\",\"message\":\"target ${_tgt} NOT FOUND\"}\n")
    message(WARNING "[DEBUG] ${_tgt} TARGET NOT FOUND")
  endif()
endforeach()
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
