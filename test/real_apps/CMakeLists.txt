add_subdirectory(gray-scott)
add_subdirectory(io_comp)
add_subdirectory(operator_comp)
add_subdirectory(metadata_comp)

#bootstraping
#jarvis_cmake()
#jarvis_repo_add(${CMAKE_SOURCE_DIR}/test/jarvis_repo)
#jarvis_load(${CMAKE_SOURCE_DIR}/test/jarvis_pipelines)
#
#add_test(Name bads COMMAND .,/oipsdfjfi)
#add_test(NAME operator_comp COMMAND jarvis run pipeline operator_comp_single_node TAG local_tests)
#add_test(NAME operator_comp COMMAND jarvis run slurm operator_comp_multi_node TAG ares_tests)
#add_test(NAME operator_comp COMMAND jarvis run pbs operator_comp_multi_node TAG polaris_tests)
#add_test(NAME operator_comp COMMAND jarvis run slurm operator_comp_multi_node queue=my_group_queue TAG pnnl_tests)
#
#ctest -D local_tests