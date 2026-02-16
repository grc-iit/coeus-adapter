/*
 * Copyright (c) 2024, Gnosis Research Center, Illinois Institute of Technology
 * All rights reserved.
 *
 * This file is part of IOWarp Core.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef HSHM_THREAD_THREAD_H_
#define HSHM_THREAD_THREAD_H_

#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

#include "hermes_shm/types/argpack.h"
#include "hermes_shm/types/bitfield.h"
#include "hermes_shm/types/numbers.h"

#if HSHM_ENABLE_PTHREADS
#include <pthread.h>
#endif
#if HSHM_ENABLE_THALLIUM
#include <thallium.hpp>
#endif
#include <thread>

namespace hshm {

/** Available threads that are mapped */
enum class ThreadType { kNone, kPthread, kArgobots, kCuda, kRocm, kStdThread };

/** Thread-local key */
union ThreadLocalKey {
#if HSHM_ENABLE_PTHREADS
  pthread_key_t pthread_key_;
#endif
#if HSHM_ENABLE_THALLIUM
  ABT_key argobots_key_;
#endif
#if HSHM_ENABLE_WINDOWS_THREADS
  DWORD windows_key_;
#endif
};

/** Thread Group Context */
struct ThreadGroupContext {
  // NOTE(llogan): Argobots supports various schedulers, etc.
  int nothing_;
};

/** Thread group */
struct ThreadGroup {
#if HSHM_ENABLE_THALLIUM
  ABT_xstream abtxstream_ = nullptr;
#endif
};

template <typename FUN, typename... Args>
struct ThreadParams {
  FUN func_;
  ArgPack<Args...> args_;

  ThreadParams(FUN &&func, Args &&...args)
      : func_(std::forward<FUN>(func)), args_(std::forward<Args>(args)...) {}
};

/** Thread */
struct Thread {
  ThreadGroup group_;
#if HSHM_ENABLE_THALLIUM
  ABT_thread abt_thread_ = nullptr;
#endif
#if HSHM_ENABLE_PTHREADS
  pthread_t pthread_thread_;
#endif
  std::thread std_thread_;
};

}  // namespace hshm

namespace hshm::thread {

/** Thread-local key */
using hshm::ThreadLocalKey;

/** Thread group */
using hshm::ThreadGroup;

/** Thread */
using hshm::Thread;

/** Thread group context */
using hshm::ThreadGroupContext;

/** Thread-local storage */
class ThreadLocalData {
 public:
  // HSHM_CROSS_FUN
  // void destroy() = 0;

  template <typename TLS>
  HSHM_CROSS_FUN static void destroy_wrap(void *data) {
    if (data) {
      // TODO(llogan): Figure out why this segfaults on exit
      //   if constexpr (std::is_base_of_v<ThreadLocalData, TLS>) {
      //     static_cast<TLS *>(data)->destroy();
      //   }
    }
  }
};

/** Represents the generic operations of a thread */
class ThreadModel {
 public:
  ThreadType type_;

 public:
  /** Initializer */
  HSHM_INLINE_CROSS_FUN
  explicit ThreadModel(ThreadType type) : type_(type) {}

  /** Get the thread model type */
  HSHM_INLINE_CROSS_FUN
  ThreadType GetType() { return type_; }
};

}  // namespace hshm::thread

#endif  // HSHM_THREAD_THREAD_H_
