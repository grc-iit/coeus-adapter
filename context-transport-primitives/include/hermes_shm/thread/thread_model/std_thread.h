#ifndef HSHM_THREAD_STD_THREAD_H_
#define HSHM_THREAD_STD_THREAD_H_

// StdThread is always available as it uses standard C++ features
#include <thread>

#include "hermes_shm/introspect/system_info.h"
#include "thread_model.h"

namespace hshm::thread {

/** Represents the generic operations of a thread */
class StdThread : public ThreadModel {
 public:
  /** Initializer */
  HSHM_INLINE_CROSS_FUN
  StdThread() : ThreadModel(ThreadType::kStdThread) {}

  /** Initialize std thread */
  HSHM_CROSS_FUN
  void Init() {}

  /** Sleep thread for a period of time */
  HSHM_CROSS_FUN
  void SleepForUs(size_t us) {
    std::this_thread::sleep_for(std::chrono::microseconds(us));
  }

  /** Yield thread time slice */
  HSHM_CROSS_FUN
  void Yield() { std::this_thread::yield(); }

  /** Create thread-local storage */
  template <typename TLS>
  HSHM_CROSS_FUN bool CreateTls(ThreadLocalKey &key, TLS *data) {
#if HSHM_IS_HOST
    return SystemInfo::CreateTls(key, (void *)data);
#else
    return false;
#endif
  }

  /** Create thread-local storage */
  template <typename TLS>
  HSHM_CROSS_FUN bool SetTls(ThreadLocalKey &key, TLS *data) {
#if HSHM_IS_HOST
    return SystemInfo::SetTls(key, (void *)data);
#else
    return false;
#endif
  }

  /** Get thread-local storage */
  template <typename TLS>
  HSHM_CROSS_FUN TLS *GetTls(const ThreadLocalKey &key) {
#if HSHM_IS_HOST
    return static_cast<TLS *>(SystemInfo::GetTls(key));
#else
    return nullptr;
#endif
  }

  /** Get the TID of the current thread */
  HSHM_CROSS_FUN
  ThreadId GetTid() { return ThreadId(SystemInfo::GetTid()); }

  /** Get the thread model type */
  HSHM_INLINE_CROSS_FUN
  ThreadType GetType() { return type_; }

  /** Create a thread group */
  HSHM_CROSS_FUN
  ThreadGroup CreateThreadGroup(const ThreadGroupContext &ctx) {
    return ThreadGroup{};
  }

  /** Spawn a thread */
  template <typename FUNC, typename... Args>
  HSHM_CROSS_FUN Thread Spawn(ThreadGroup &group, FUNC &&func, Args &&...args) {
    Thread thread;
    thread.group_ = group;
    thread.std_thread_ =
        std::thread(std::forward<FUNC>(func), std::forward<Args>(args)...);
    return thread;
  }

  /** Join a thread */
  HSHM_CROSS_FUN
  void Join(Thread &thread) { thread.std_thread_.join(); }

  /** Set CPU affinity for thread */
  HSHM_CROSS_FUN
  void SetAffinity(Thread &thread, int cpu_id) {}
};

}  // namespace hshm::thread

#endif  // HSHM_THREAD_STD_THREAD_H_
