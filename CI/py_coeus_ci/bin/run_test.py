#!/usr/bin/env python3

import sys, os
import pathlib

# Need to update script to COEUS project

if __name__ == '__main__':
    if len(sys.argv) != 4:
        print("USAGE: ./run_test [TEST_TYPE] [TEST_NAME] [CMAKE_BINARY_DIR]")
        exit(1)
    test_type = sys.argv[1]
    test_name = sys.argv[2]
    cmake_binary_dir = sys.argv[3]
    address_sanitizer = False
    # The root of Coeus
    COEUS_ROOT = str(pathlib.Path(__file__).parent.
                      parent.parent.parent.resolve())

    ADAPTER_TEST_ROOT = f"{COEUS_ROOT}/adapter/test" # Do we need to add the directory?

    # Ensure that all calls beneath know how to resolve jarvis_util
    sys.path.insert(0, f"{COEUS_ROOT}/CI/jarvis-util")

    # Ensure subsequent classes know how to resolve py_coeus_ci package
    sys.path.insert(0, f"{COEUS_ROOT}/CI/py_coeus_ci")

    # Choose which unit test file to (we are just going to test 'Native')
    if test_type == 'stdio':
        pkg_dir = f"{ADAPTER_TEST_ROOT}/{test_type}"
    elif test_type == 'posix':
        pkg_dir = f"{ADAPTER_TEST_ROOT}/{test_type}"
    elif test_type == 'mpiio':
        pkg_dir = f"{ADAPTER_TEST_ROOT}/{test_type}"
    elif test_type == 'vfd':
        pkg_dir = f"{ADAPTER_TEST_ROOT}/{test_type}"
    elif test_type == 'native':
        pkg_dir = f"{COEUS_ROOT}/test/unit"
    elif test_type == 'data_stager':
        pkg_dir = f"{COEUS_ROOT}/data_stager/test"
    elif test_type == 'kvstore':
        pkg_dir = f"{COEUS_ROOT}/adapter/kvstore"
    elif test_type == 'java_wrapper':
        pkg_dir = f"{COEUS_ROOT}/wrapper/java"
    else:
        raise Exception("Could not find the unit test")
    # Load the unit test
    from jarvis_util.util.naming import to_camel_case
    from jarvis_util.util.import_mod import load_class
    test_cls = load_class(f"tests",
                          pkg_dir,
                          to_camel_case(f"{test_type}_test_manager"))
    tests = test_cls(COEUS_ROOT, cmake_binary_dir, address_sanitizer)
    tests.call(test_name)
