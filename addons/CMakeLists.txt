#------------------------------------------------------------------------------
# Coeus-adapter add-on operators (opt-in; OFF by default).
#
# Self-contained: builds ONLY when -DCOEUS_ENABLE_OPERATORS=ON. Reuses the
# engine's existing, UNMODIFIED comms sources (CTEHermes.cc / CTETagClient.cc)
# to read step-tagged CTE blobs out-of-band. Nothing here is linked into
# hermes_engine, and hermes_engine.cc is never touched.
#------------------------------------------------------------------------------

option(COEUS_ENABLE_TDERIV "Build the time-derivative operator" ON)

# NOTE: there is intentionally no hash target here. hash() is an ADIOS2 derived
# variable (provided by the state-diff-enabled ADIOS2 fork) and is consumed by
# the engine's existing ComputeDerivedVariables() path — coeus-adapter links
# neither state-diff nor Kokkos. See addons/HASH.md.

# Shared consumer runtime: the CTE step-blob reader + the engine's comms layer.
add_library(coeus_ops_common STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/common/StepBlobReader.cc
    ${CMAKE_SOURCE_DIR}/include/comms/CTEHermes.cc
    ${CMAKE_SOURCE_DIR}/include/comms/CTETagClient.cc)

target_include_directories(coeus_ops_common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include              # comms/*.h
    ${ADIOS2_INCLUDE_DIR}
    ${SQLite3_INCLUDE_DIRS})

target_link_libraries(coeus_ops_common PUBLIC
    clio::run::cxx
    clio::cte::core_client
    ${ADIOS2_LIBRARIES})

# CTE tasks use C++20 coroutines, same as hermes_engine.
set_target_properties(coeus_ops_common PROPERTIES
    CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
target_compile_options(coeus_ops_common PRIVATE -fcoroutines)

if(COEUS_ENABLE_TDERIV)
  add_subdirectory(tderiv)
endif()
