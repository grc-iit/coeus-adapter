//
// Created by llogan on 26/10/24.
//

#ifndef HSHM_SHM_INCLUDE_HSHM_SHM_THREAD_LOCK_SPIN_LOCK_H_
#define HSHM_SHM_INCLUDE_HSHM_SHM_THREAD_LOCK_SPIN_LOCK_H_

#include "hermes_shm/types/atomic.h"
#include "hermes_shm/types/numbers.h"

namespace hshm {

struct SpinLock {
  ipc::atomic<hshm::min_u64> lock_;
  ipc::atomic<hshm::min_u64> head_;
  ipc::atomic<hshm::min_u32> try_lock_;
#ifdef HSHM_DEBUG_LOCK
  u32 owner_;
#endif

  /** Default constructor */
  HSHM_INLINE_CROSS_FUN
  SpinLock() : lock_(0), head_(0), try_lock_(0) {}

  /** Copy constructor */
  HSHM_INLINE_CROSS_FUN
  SpinLock(const SpinLock &other) {}

  /** Explicit initialization */
  HSHM_INLINE_CROSS_FUN
  void Init() { lock_ = 0; }

  /** Acquire lock */
  HSHM_INLINE_CROSS_FUN
  void Lock(u32 owner) {
    min_u64 tkt = lock_.fetch_add(1);
    do {
      for (int i = 0; i < 1; ++i) {
        if (tkt == head_.load()) {
          return;
        }
      }
    } while (true);
  }

  /** Try to acquire the lock */
  HSHM_INLINE_CROSS_FUN
  bool TryLock(u32 owner) {
    if (try_lock_.fetch_add(1) > 0 || lock_.load() > head_.load()) {
      try_lock_.fetch_sub(1);
      return false;
    }
    Lock(owner);
    return true;
  }

  /** Unlock */
  HSHM_INLINE_CROSS_FUN
  void Unlock() {
#ifdef HSHM_DEBUG_LOCK
    owner_ = 0;
#endif
    head_.fetch_add(1);
  }
};

struct ScopedSpinLock {
  SpinLock &lock_;
  bool is_locked_;

  /** Acquire the mutex */
  HSHM_INLINE_CROSS_FUN explicit ScopedSpinLock(SpinLock &lock, uint32_t owner)
      : lock_(lock), is_locked_(false) {
    Lock(owner);
  }

  /** Release the mutex */
  HSHM_INLINE_CROSS_FUN
  ~ScopedSpinLock() { Unlock(); }

  /** Explicitly acquire the mutex */
  HSHM_INLINE_CROSS_FUN
  void Lock(uint32_t owner) {
    if (!is_locked_) {
      lock_.Lock(owner);
      is_locked_ = true;
    }
  }

  /** Explicitly try to lock the mutex */
  HSHM_INLINE_CROSS_FUN
  bool TryLock(uint32_t owner) {
    if (!is_locked_) {
      is_locked_ = lock_.TryLock(owner);
    }
    return is_locked_;
  }

  /** Explicitly unlock the mutex */
  HSHM_INLINE_CROSS_FUN
  void Unlock() {
    if (is_locked_) {
      lock_.Unlock();
      is_locked_ = false;
    }
  }
};

}  // namespace hshm

namespace hshm::ipc {

using hshm::ScopedSpinLock;
using hshm::SpinLock;

}  // namespace hshm::ipc

#endif  // HSHM_SHM_INCLUDE_HSHM_SHM_THREAD_LOCK_SPIN_LOCK_H_
