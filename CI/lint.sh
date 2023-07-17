#!/bin/bash
PROJECT_ROOT=$1

echo "linting"
cpplint --recursive \
--exclude="${PROJECT_ROOT}/build-remote" \
--exclude="/tmp/coeus-adapter/build-remote" \
--exclude="${PROJECT_ROOT}/build_docker" \
--exclude="/tmp/coeus-adapter/build_docker" \
--exclude="${PROJECT_ROOT}/CMake" \
--exclude="${PROJECT_ROOT}/CI" \
--exclude="${PROJECT_ROOT}/external_libraries" \
--exclude="${PROJECT_ROOT}/test/unit" \
--exclude="${PROJECT_ROOT}/test/real_apps/gray-scott" \
"${PROJECT_ROOT}"