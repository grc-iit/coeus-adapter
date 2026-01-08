/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Distributed under BSD 3-Clause license.                                   *
 * Copyright by The HDF Group.                                               *
 * Copyright by the Illinois Institute of Technology.                        *
 * All rights reserved.                                                      *
 *                                                                           *
 * This file is part of Hermes. The full Hermes copyright notice, including  *
 * terms governing use, modification, and redistribution, is contained in    *
 * the COPYING file, which can be found at the top directory. If you do not  *
 * have access to the file, you may request a copy from help@hdfgroup.org.   *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef HSHM_ERRORS_H
#define HSHM_ERRORS_H

#ifdef __cplusplus

#include "hermes_shm/util/error.h"

namespace hshm {
const Error MEMORY_BACKEND_REPEATED(
    "Attempted to register two backends "
    "with the same id");
const Error TOO_MANY_ALLOCATORS("Too many allocators");
const Error NOT_IMPLEMENTED("{} not implemented");

const Error SHMEM_CREATE_FAILED("Failed to allocate SHMEM");
const Error SHMEM_RESERVE_FAILED("Failed to reserve SHMEM");
const Error SHMEM_NOT_SUPPORTED("Attempting to deserialize a non-shm backend");
const Error MEMORY_BACKEND_CREATE_FAILED("Failed to load memory backend");
const Error MEMORY_BACKEND_NOT_FOUND("Failed to find the memory backend");
const Error OUT_OF_MEMORY(
    "could not allocate memory of size {} from heap of size {}");
const Error INVALID_FREE("could not free memory");
const Error DOUBLE_FREE("Freeing the same memory twice: {}!");

const Error IPC_ARGS_NOT_SHM_COMPATIBLE("Args are not compatible with SHM");

const Error UNORDERED_MAP_CANT_FIND("Could not find key in unordered_map");
const Error KEY_SET_OUT_OF_BOUNDS("Too many keys in the key set");

const Error ARGPACK_INDEX_OUT_OF_BOUNDS("Argpack index out of bounds");
}  // namespace hshm

#endif

#endif  // HSHM_ERRORS_H
