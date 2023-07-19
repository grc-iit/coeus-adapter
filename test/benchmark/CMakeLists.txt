#------------------------------------------------------------------------------
# Enable Benchmark Testing
#------------------------------------------------------------------------------
#function(pytest_benchmark test_type test_name label)
#    set(script ${CMAKE_SOURCE_DIR}/CI/py_coeus_ci/bin/run_bench_test.py)
#    foreach(num_procs RANGE 1 640)
#        add_test(NAME ${test_name}_${num_procs}
#                COMMAND python3 ${script} ${test_type} ${test_name} ${num_procs} ${CMAKE_BINARY_DIR})
#        set_tests_properties(${test_name}_${num_procs} PROPERTIES LABELS ${label})
#    endforeach()
#endfunction()

pytest_benchmark(native test_gray_scott_simulation_file_bench benchmark)
