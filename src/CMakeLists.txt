include_directories(${CMAKE_SOURCE_DIR})

#------------------------------------------------------------------------------
# Set sources
#------------------------------------------------------------------------------
set(HERMES_ENGINE_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/hermes_engine.cc)

#------------------------------------------------------------------------------
# Libraries
#------------------------------------------------------------------------------
add_library(hermes_engine ${HERMES_ENGINE_SRCS} ../include/coeus/metadata_serializer.h)
add_dependencies(hermes_engine spdlog)
target_include_directories(hermes_engine PUBLIC
        ../include
        ${Hermes_INCLUDE_DIR}
        ${ADIOS2_INCLUDE_DIR})
target_link_libraries(hermes_engine LINK_PUBLIC
        ${Hermes_LIBRARIES}
        ${ADIOS2_LIBRARIES}
        spdlog
        cereal)
#-----------------------------------------------------------------------------
# Add Target(s) to CMake Install
#-----------------------------------------------------------------------------
#install(
#  TARGETS
#    hermes_engine
#  EXPORT
#    ${COEUS_EXPORTED_TARGETS}
#  LIBRARY DESTINATION ${COEUS_INSTALL_LIB_DIR}
#  ARCHIVE DESTINATION ${COEUS_INSTALL_LIB_DIR}
#  RUNTIME DESTINATION ${COEUS_INSTALL_BIN_DIR}
#)
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
