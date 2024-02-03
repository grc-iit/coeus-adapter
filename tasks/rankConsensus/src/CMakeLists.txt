#------------------------------------------------------------------------------
# Build Small Message Task Library
#------------------------------------------------------------------------------
add_library(rankConsensus SHARED
        rankConsensus.cc)
target_link_libraries(rankConsensus ${Hermes_RUNTIME_LIBRARIES})

#------------------------------------------------------------------------------
# Install Small Message Task Library
#------------------------------------------------------------------------------
#install(
#        TARGETS
#        rankConsensus
#        EXPORT
#        ${COEUS_EXPORTED_TARGETS}
#        LIBRARY DESTINATION ${HERMES_INSTALL_LIB_DIR}
#        ARCHIVE DESTINATION ${HERMES_INSTALL_LIB_DIR}
#        RUNTIME DESTINATION ${HERMES_INSTALL_BIN_DIR}
#)

#-----------------------------------------------------------------------------
# Add Target(s) to CMake Install for import into other projects
#-----------------------------------------------------------------------------
#install(
#        EXPORT
#        ${COEUS_EXPORTED_TARGETS}
#        DESTINATION
#        ${HERMES_INSTALL_DATA_DIR}/cmake/hermes
#        FILE
#        ${COEUS_EXPORTED_TARGETS}.cmake
#)

#-----------------------------------------------------------------------------
# Export all exported targets to the build tree for use by parent project
#-----------------------------------------------------------------------------
set(HERMES_EXPORTED_LIBS
        rankConsensus
        ${HERMES_EXPORTED_LIBS})
if(NOT HERMES_EXTERNALLY_CONFIGURED)
    EXPORT (
            TARGETS
            ${HERMES_EXPORTED_LIBS}
            FILE
            ${COEUS_EXPORTED_TARGETS}.cmake
    )
endif()

#------------------------------------------------------------------------------
# Coverage
#------------------------------------------------------------------------------
if(HERMES_ENABLE_COVERAGE)
    set_coverage_flags(rankConsensus)
endif()
