add_executable(currentStep currentStep.cc)
add_dependencies(currentStep hermes_engine)
target_link_libraries(currentStep PRIVATE hermes_engine MPI::MPI_CXX)
pytest(native test_currentStep)

#add_test(NAME currentStep_test
#       COMMAND currentStep)
#MESSAGE(STATUS "CMAKE_BINARY_DIR: ${CMAKE_BINARY_DIR}")
#set_tests_properties(currentStep_test PROPERTIES
#       ENVIRONMENT "LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/bin:$ENV{LD_LIBRARY_PATH}")