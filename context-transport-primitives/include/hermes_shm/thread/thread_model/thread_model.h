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
