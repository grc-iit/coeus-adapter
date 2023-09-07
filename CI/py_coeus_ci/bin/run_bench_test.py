#!/usr/bin/env python3
import sys, os
import pathlib

if __name__ == '__main__':
    if len(sys.argv) != 5:
        print("USAGE: ./run_bench_test [TEST_TYPE] [TEST_NAME] [CMAKE_BINARY_DIR] [NUM_PROCESSES]")
        exit(1)
    test_type = sys.argv[1]
    test_name = sys.argv[2]
    cmake_binary_dir = sys.argv[3]
    num_processes = sys.argv[4]
    address_sanitizer = False

    # The root of Coeus
    COEUS_ROOT = str(pathlib.Path(__file__).parent.parent.parent.parent.resolve())
    ADAPTER_TEST_ROOT = f"{COEUS_ROOT}/adapter/test"

    # Ensure that all calls beneath know how to resolve jarvis_util
    sys.path.insert(0, f"{COEUS_ROOT}/CI/jarvis-util")

    # Ensure subsequent classes know how to resolve py_coeus_ci package
    sys.path.insert(0, f"{COEUS_ROOT}/CI/py_coeus_ci")

    # Unit test file to load
    if test_type == 'native':
        pkg_dir = f"{COEUS_ROOT}/test/benchmark"
    else:
        raise Exception("Could not find the unit test")

    # Load the unit test
    from jarvis_util.util.naming import to_camel_case
    from jarvis_util.util.import_mod import load_class

    test_cls = load_class(f"benchmark_tests", pkg_dir, to_camel_case(f"{test_type}_test_manager_bench"))
    tests = test_cls(COEUS_ROOT, cmake_binary_dir, address_sanitizer)
    tests.call(test_name, num_processes)