#!/bin/bash

HSHM_ROOT=$1

cpplint --recursive \
--exclude="${HSHM_ROOT}/include/hermes_shm/constants/singleton_macros.h" \
--exclude="${HSHM_ROOT}/include/hermes_shm/data_structures/internal/template" \
--exclude="${HSHM_ROOT}/include/hermes_shm/data_structures/internal/shm_container_macro.h" \
--exclude="${HSHM_ROOT}/src/singleton.cc" \
--exclude="${HSHM_ROOT}/src/data_structure_singleton.cc" \
--exclude="${HSHM_ROOT}/include/hermes_shm/util/formatter.h" \
--exclude="${HSHM_ROOT}/include/hermes_shm/util/errors.h" \
"${HSHM_ROOT}/src" "${HSHM_ROOT}/include" "${HSHM_ROOT}/test"