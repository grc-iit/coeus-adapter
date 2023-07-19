# CMake generated Testfile for 
# Source directory: /tmp/tmp.5TUlmkfNlZ/test/unit/metadata
# Build directory: /tmp/tmp.5TUlmkfNlZ/build-remote/test/unit/metadata
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(metadata_test "/tmp/tmp.5TUlmkfNlZ/build-remote/bin/metadata")
set_tests_properties(metadata_test PROPERTIES  ENVIRONMENT "LD_LIBRARY_PATH=/tmp/tmp.5TUlmkfNlZ/build-remote/bin:/tmp/tmp.5TUlmkfNlZ/build-remote/bin/:/mnt/repo/software/spack/spack/opt/spack/linux-ubuntu22.04-skylake_avx512/gcc-11.3.0/hermes-master-feow7upwxbuwtlutwrs4bxsp5kyytmqj/lib64:/mnt/repo/software/spack/spack/opt/spack/linux-ubuntu22.04-skylake_avx512/gcc-11.3.0/hermes-master-feow7upwxbuwtlutwrs4bxsp5kyytmqj/lib" _BACKTRACE_TRIPLES "/tmp/tmp.5TUlmkfNlZ/test/unit/metadata/CMakeLists.txt;14;add_test;/tmp/tmp.5TUlmkfNlZ/test/unit/metadata/CMakeLists.txt;0;")
add_test(test_split_metadata "python3" "/tmp/tmp.5TUlmkfNlZ/CI/py_coeus_ci/bin/run_test.py" "native" "test_split_metadata" "/tmp/tmp.5TUlmkfNlZ/build-remote")
set_tests_properties(test_split_metadata PROPERTIES  _BACKTRACE_TRIPLES "/tmp/tmp.5TUlmkfNlZ/CMakeLists.txt;124;add_test;/tmp/tmp.5TUlmkfNlZ/test/unit/metadata/CMakeLists.txt;17;pytest;/tmp/tmp.5TUlmkfNlZ/test/unit/metadata/CMakeLists.txt;0;")
