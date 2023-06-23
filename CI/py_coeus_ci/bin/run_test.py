#!/usr/bin/env python3
import sys, os
import pathlib

if __name__ == '__main__':
    if len(sys.argv) != 4:
        print("USAGE: ./run_test [TEST_TYPE] [TEST_NAME] [CMAKE_BINARY_DIR]")
        exit(1)
    test_type = sys.argv[1]
    test_name = sys.argv[2]
    cmake_binary_dir = sys.argv[3]
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
        pkg_dir = f"{COEUS_ROOT}/test"
    else:
        raise Exception("Could not find the unit test")

    # Load the unit test
    from jarvis_util.util.naming import to_camel_case
    from jarvis_util.util.import_mod import load_class

    # Modify the arguments for the pdf-calc test
   # if test_name == 'test_pdf_calc':
    #     input_file = ''
    #     output_file = ''
    #    num_bins = 1000
    #    output_inputdata = 'YES'
    #    test_args = [input_file, output_file, str(num_bins), output_inputdata]
    # else:
    #    test_args = []

    test_cls = load_class(f"native_tests", pkg_dir, to_camel_case(f"{test_type}_test_manager"))
    tests = test_cls(COEUS_ROOT, cmake_binary_dir, address_sanitizer)
    #tests.call(test_name, *test_args)
    tests.call(test_name)