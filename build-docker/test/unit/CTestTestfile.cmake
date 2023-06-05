# CMake generated Testfile for 
# Source directory: /tmp/coeus-adapter/test/unit
# Build directory: /tmp/coeus-adapter/build-docker/test/unit
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(basic_plugin_test "/tmp/coeus-adapter/build-docker/bin/basic" "/tmp/coeus-adapter/build-docker/bin")
set_tests_properties(basic_plugin_test PROPERTIES  ENVIRONMENT "LD_LIBRARY_PATH=/tmp/coeus-adapter/build-docker/bin:" _BACKTRACE_TRIPLES "/tmp/coeus-adapter/test/unit/CMakeLists.txt;5;add_test;/tmp/coeus-adapter/test/unit/CMakeLists.txt;0;")
