# CMake generated Testfile for 
# Source directory: /tmp/tmp.5TUlmkfNlZ/test/real_apps/gray-scott
# Build directory: /tmp/tmp.5TUlmkfNlZ/build-remote/test/real_apps/gray-scott
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(test_gray_scott_analysis_file_parallel "python3" "/tmp/tmp.5TUlmkfNlZ/CI/py_coeus_ci/bin/run_test.py" "native" "test_gray_scott_analysis_file_parallel" "/tmp/tmp.5TUlmkfNlZ/build-remote")
set_tests_properties(test_gray_scott_analysis_file_parallel PROPERTIES  _BACKTRACE_TRIPLES "/tmp/tmp.5TUlmkfNlZ/CMakeLists.txt;124;add_test;/tmp/tmp.5TUlmkfNlZ/test/real_apps/gray-scott/CMakeLists.txt;50;pytest;/tmp/tmp.5TUlmkfNlZ/test/real_apps/gray-scott/CMakeLists.txt;0;")
