#------------------------------------------------------------------------------#
# Distributed under the OSI-approved Apache License, Version 2.0.  See
# accompanying file Copyright.txt for details.
#------------------------------------------------------------------------------#
# Define the BINARY_DIR variable
set(BINARY_DIR ${CMAKE_BINARY_DIR})

# Pass the BINARY_DIR as a preprocessor definition
add_definitions(-DBINARY_DIR="${BINARY_DIR}")


add_executable(adios2-gray-scott
  simulation/main.cpp
  simulation/gray-scott.cpp
  simulation/settings.cpp
  simulation/writer.cpp
  simulation/restart.cpp
)
target_link_libraries(adios2-gray-scott adios2::adios2 hermes_engine MPI::MPI_C)
target_include_directories(adios2-gray-scott PRIVATE
        ${Hermes_INCLUDE_DIRS}
        ${ADIOS2_INCLUDE_DIR}
        )
install(TARGETS adios2-gray-scott 
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

add_executable(adios2-pdf-calc analysis/pdf-calc.cpp)
target_link_libraries(adios2-pdf-calc adios2::adios2 hermes_engine MPI::MPI_C)
target_include_directories(adios2-pdf-calc PRIVATE
        ${Hermes_INCLUDE_DIRS}
        ${ADIOS2_INCLUDE_DIR}
        )
install(TARGETS adios2-pdf-calc 
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})


install(FILES "adios2.xml" "adios2-inline-plugin.xml" "adios2-fides-staging.xml"
                           "visit-bp4.session" "visit-bp4.session.gui" 
                           "visit-sst.session" "visit-sst.session.gui"                  
                           "simulation/settings-files.json"
                           "simulation/settings-staging.json"
                           "simulation/settings-inline.json"
                           "plot/decomp.py" "plot/gsplot.py" "plot/pdfplot.py"
                           "README.md" 
        DESTINATION ${CMAKE_INSTALL_PREFIX}/share/adios2-examples/gray-scott) 

install(DIRECTORY "catalyst"
        DESTINATION ${CMAKE_INSTALL_PREFIX}/share/adios2-examples/gray-scott) 


install(PROGRAMS "cleanup.sh" 
        DESTINATION ${CMAKE_INSTALL_PREFIX}/share/adios2-examples/gray-scott) 

pytest(native test_gray_scott_simulation_file)
pytest(native test_gray_scott_analysis_file)
pytest(native test_gray_scott_simulation_file_parallel)
pytest(native test_gray_scott_analysis_file_parallel)

pytest(native test_gray_scott_simulation_hermes)
pytest(native test_gray_scott_analysis_hermes)
pytest(native test_gray_scott_simulation_hermes_parallel)
pytest(native test_gray_scott_analysis_hermes_parallel)