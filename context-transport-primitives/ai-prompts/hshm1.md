Make it so gcc stops at the first compiler error.

# Factoring out External Library Headers
Edit certain C++ headers relying on external libraries to be factored out with compile-time macros that can be set from the cmake options. There are several major locations in include/hermes_shm:
1. lightbeam: transports should be guarded with macros. E.g., zmq should be guarded with HSHM_ENABLE_ZMQ.
2. thread/thread_model: Make each thread model (e.g., pthread.h) is guarded. Check thread_model.h to see the macros for that. Remove repetitive header guarding from thread_model.h.
3. util/compress: each compression library should be guarded with HSHM_ENABLE_COMPRESS.
4. util/encrypt: each encryption library should be guarded with HSHM_ENABLE_ENCRYPT. 
5. memory/backend: each gpu backend should be guarded with HSHM_ENABLE_CUDA || HSHM_ENABLE_ROCM

For example for include/hermes_shm/lightbeam/libfabric_transport.h:
```cpp
#pragma once
#if HSHM_ENABLE_LIBFABRIC  // ADD ME
#include <rdma/fabric.h>
#include <rdma/fi_endpoint.h>
#include <rdma/fi_errno.h>
#include <rdma/fi_cm.h>
#include <cstring>
#include <queue>
#include <mutex>
#include <memory>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <vector>
#include "lightbeam.h"

// All other existing code

#endif  // ADD ME
```

Make it so each factory file places the macro guards around corresponding switch-case statements. For example, in lightbeam, it should be:
```cpp
#if HSHM_ENABLE_ZMQ
    case Transport::kZeroMq:
      return std::make_unique<ZeroMqClient>(
          addr, protocol.empty() ? "tcp" : protocol,
          port == 0 ? 8192 : port);
#endif HSHM_ENABLE_ZMQ
```

# Improving Macro Definitions
Replace ``__HSHM_IS_COMPILING__`` with ``HSHM_ENABLE_DLL_EXPORT``. Move this as a compile-time constant to CMakeLists.txt. It should be a private constant, not public. Make sure to fix the HSHM_DLL ifdef statements in include/hermes_shm/constants/macros.h to use ``#if HSHM_ENABLE_DLL_EXPORT`` instead.

Make HSHM_IS_HOST and HSHM_IS_GPU be set to 0 and 1. They should be defined always, regardless of if CUDA / ROCM are defined. Make sure that 

Let's remove constants/settings.h and settings.h_templ and replace with macro targets in CMakeLists.txt. Remove the settings_templ compilation in the CMakeLists.txt. Make a CMake function for the target_compile_definitions. The resulting target_compile_definitions should be roughly like this, though there are more than these macros:
```cmake
target_compile_definitions(${target} PUBLIC
        HSHM_COMPILER_MSVC=$<BOOL:${HSHM_COMPILER_MSVC}>
        HSHM_COMPILER_GNU=$<BOOL:${HSHM_COMPILER_GNU}>
        HSHM_ENABLE_MPI=$<BOOL:${HSHM_ENABLE_MPI}>
        HSHM_ENABLE_OPENMP=$<BOOL:${HSHM_ENABLE_OPENMP}>
        HSHM_ENABLE_THALLIUM=$<BOOL:${HSHM_ENABLE_THALLIUM}>)
```
Make sure that most of the macros are public and others are private. E.g., HSHM_ENABLE_CUDA should be private.  Ensure that you remove the settings.h compiling from CMakeLists.txt. Ensure that the target_compile_definitions function is called for each hshm target that gets built, including cxx, cudacxx, rocmcxx_gpu, and rocmcxx_host.

Convert every ``HSHM_ENABLE*`` and ``HSHM_IS*`` macro to use ``#if`` instead of ``#ifdef`` and ``#if defined``. Move HSHM_DEFAULT_THREAD_MODEL, HSHM_DEFAULT_THREAD_MODEL_GPU, HSHM_DEFAULT_ALLOC_T to CMakeLists.txt as compile-time constants. Remove them from the macros.h file afterwards.  Check every single header file in include/hermes_shm for this. 

Ensure that every HSHM_IS* macro is always be defined. All these macros are initially defined in macros.h.

# Improving Header Guards
Ensure that hermes_shm/constants/macros.h is included in every header file. Let's use #pragma once to replace header guards in each header file in include/hermes_shm. All header guards begin with ``#ifndef``. Typically these are the first ifdefs in the file. Not all ifndefs should be replaced.

# Comprehensive Include
Make it so ``#include <hermes_shm/hermes_shm.h>`` includes every header in include/hermes_shm. Since the headers now have the guards, this should be safe to do. Make it so the unit tests include this file.

# Add MPI and OpenMP Macros
We should rename the variable B