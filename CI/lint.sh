#!/bin/bash
PROJECT_ROOT=$1

cpplint --recursive \
--exclude="${PROJECT_ROOT}/build" \
--exclude="${PROJECT_ROOT}/build_docker" \
--exclude="${PROJECT_ROOT}/CMake" \
--exclude="${PROJECT_ROOT}/CI" \
"${PROJECT_ROOT}"