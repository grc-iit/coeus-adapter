add_test([=[HelloTest.BasicAssertions]=]  /home/lukemartinlogan/Documents/Projects/PhD/coeus-adapter/cmake-build-debug/bin/hello_test [==[--gtest_filter=HelloTest.BasicAssertions]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[HelloTest.BasicAssertions]=]  PROPERTIES WORKING_DIRECTORY /home/lukemartinlogan/Documents/Projects/PhD/coeus-adapter/cmake-build-debug/test/unit/gtest SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==])
set(  hello_test_TESTS HelloTest.BasicAssertions)
