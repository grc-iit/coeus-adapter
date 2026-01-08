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

#ifndef HSHM_THREAD_ARGOBOTS_H_
#define HSHM_THREAD_ARGOBOTS_H_

#if HSHM_ENABLE_THALLIUM

#include <errno.h>

#include <thallium.hpp>

#include "hermes_shm/introspect/system_info.h"
#include "hermes_shm/util/errors.h"
#include "thread_model.h"

namespace hshm::thread {

class Argobots : public ThreadModel {
 public:
  /** Default constructor */
  HSHM_INLINE_CROSS_FUN
  Argobots() : ThreadModel(ThreadType::kArgobots) {}

  /** Destructor */
  HSHM_CROSS_FUN
  ~Argobots() = default;

  /** Initialize Argobots */
  HSHM_CROSS_FUN
  void Init() { ABT_init(0, nullptr); }

  /** Yield the current thread for a period of time */
  HSHM_CROSS_FUN
  void SleepForUs(size_t us) {
    /**
     * TODO(llogan): make this API flexible enough to support argobots fully
     * tl::thread::self().sleep(*HSHM->rpc_.server_engine_,
                               HSHM->server_config_.borg_.blob_reorg_period_);
     */
#if HSHM_IS_HOST
    usleep(us);
#endif
  }

  /** Yield thread time slice */
  HSHM_CROSS_FUN
  void Yield() {
#if HSHM_IS_HOST
    ABT_thread_yield();
#endif
  }

  /** Create thread-local storage */
  template <typename TLS>
  HSHM_CROSS_FUN bool CreateTls(ThreadLocalKey &key, TLS *data) {
#if HSHM_IS_HOST
    int ret = ABT_key_create(ThreadLocalData::template destroy_wrap<TLS>,
                             &key.argobots_key_);
    if (ret != ABT_SUCCESS) {
      return false;
    }
    return SetTls(key, data);
#else
    return false;
#endif
  }

  /** Create thread-local storage */
  template <typename TLS>
  HSHM_CROSS_FUN bool SetTls(ThreadLocalKey &key, TLS *data) {
#if HSHM_IS_HOST
    int ret = ABT_key_set(key.argobots_key_, data);
    return ret == ABT_SUCCESS;
#else
    return false;
#endif
  }

  /** Get thread-local storage */
  template <typename TLS>
  HSHM_CROSS_FUN TLS *GetTls(const ThreadLocalKey &key) {
#if HSHM_IS_HOST
    TLS *data;
    ABT_key_get(key.argobots_key_, (void **)&data);
    return (TLS *)data;
#else
    return nullptr;
#endif
  }

  /** Get the TID of the current thread */
  HSHM_CROSS_FUN
  ThreadId GetTid() {
#if HSHM_IS_HOST
    ABT_thread thread;
    ABT_thread_id tid;
    ABT_thread_self(&thread);
    ABT_thread_get_id(thread, &tid);
    return ThreadId{tid};
#else
    return ThreadId{0};
#endif
  }

  /** Create a thread group */
  HSHM_CROSS_FUN
  ThreadGroup CreateThreadGroup(const ThreadGroupContext &ctx) {
#if HSHM_IS_HOST
    ABT_xstream xstream;
    ABT_xstream_create(ABT_SCHED_NULL, &xstream);
    return ThreadGroup{xstream};
#else
    return ThreadGroup{nullptr};
#endif
  }

  /** Spawn a thread */
  template <typename FUNC, typename... Args>
  HSHM_CROSS_FUN Thread Spawn(ThreadGroup &group, FUNC &&func, Args &&...args) {
#if HSHM_IS_HOST
    Thread thread;
    ThreadParams<FUNC, Args...> *params = new ThreadParams<FUNC, Args...>(
        std::forward<FUNC>(func), std::forward<Args>(args)...);
    thread.group_ = group;
    ABT_thread_create_on_xstream(group.abtxstream_, SpawnWrapper<FUNC, Args...>,
                                 (void *)params, ABT_THREAD_ATTR_NULL,
                                 &thread.abt_thread_);
    return thread;
#else
    return Thread{};
#endif
  }

  /** Wrapper for spawning a thread */
  template <typename FUNC, typename... Args>
  static void SpawnWrapper(void *arg) {
    ThreadParams<FUNC, Args...> *params =
        static_cast<ThreadParams<FUNC, Args...> *>(arg);
    PassArgPack::Call(std::forward<ArgPack<Args...>>(params->args_),
                      std::forward<FUNC>(params->func_));
    delete params;
  }

  /** Join a thread */
  HSHM_CROSS_FUN
  void Join(Thread &thread) {
#if HSHM_IS_HOST
    ABT_thread_join(thread.abt_thread_);
#endif
  }

  /** Set CPU affinity for thread */
  HSHM_CROSS_FUN
  void SetAffinity(Thread &thread, int cpu_id) {
#if HSHM_IS_HOST
    ABT_xstream_set_affinity(thread.group_.abtxstream_, 1, &cpu_id);
#endif
  }
};

}  // namespace hshm::thread

#endif  // HSHM_ENABLE_THALLIUM

#endif  // HSHM_THREAD_ARGOBOTS_H_
