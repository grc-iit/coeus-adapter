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

#ifndef HSHM_TIMER_H
#define HSHM_TIMER_H

#include <chrono>
#include <functional>
#include <vector>

#include "hermes_shm/constants/macros.h"
// #include "hermes_shm/data_structures/internal/shm_archive.h"
#include "singleton.h"

namespace hshm {

template <typename T>
class TimepointBase {
 public:
  std::chrono::time_point<T> start_;

 public:
  HSHM_INLINE_CROSS_FUN void Now() {
#if HSHM_IS_HOST
    start_ = T::now();
#endif
  }
  HSHM_INLINE_CROSS_FUN double GetNsecFromStart(TimepointBase &now) const {
#if HSHM_IS_HOST
    double elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
                         now.start_ - start_)
                         .count();
    return elapsed;
#else
    return 0;
#endif
  }
  HSHM_INLINE_CROSS_FUN double GetUsecFromStart(TimepointBase &now) const {
    return GetNsecFromStart(now) / 1000;
  }
  HSHM_INLINE_CROSS_FUN double GetMsecFromStart(TimepointBase &now) const {
    return GetNsecFromStart(now) / 1000000;
  }
  HSHM_INLINE_CROSS_FUN double GetSecFromStart(TimepointBase &now) const {
    return GetNsecFromStart(now) / 1000000000;
  }
  HSHM_INLINE_CROSS_FUN double GetNsecFromStart() const {
#if HSHM_IS_HOST
    std::chrono::time_point<T> end = T::now();
    double elapsed =
        (double)std::chrono::duration_cast<std::chrono::nanoseconds>(end -
                                                                     start_)
            .count();
    return elapsed;
#else
    return 0;
#endif
  }
  HSHM_INLINE_CROSS_FUN double GetUsecFromStart() const {
    return GetNsecFromStart() / 1000;
  }
  HSHM_INLINE_CROSS_FUN double GetMsecFromStart() const {
    return GetNsecFromStart() / 1000000;
  }
  HSHM_INLINE_CROSS_FUN double GetSecFromStart() const {
    return GetNsecFromStart() / 1000000000;
  }
};

class NsecTimer {
 public:
  double time_ns_;

 public:
  NsecTimer() : time_ns_(0) {}

  HSHM_INLINE_CROSS_FUN double GetNsec() const { return time_ns_; }
  HSHM_INLINE_CROSS_FUN double GetUsec() const { return time_ns_ / 1000; }
  HSHM_INLINE_CROSS_FUN double GetMsec() const { return time_ns_ / 1000000; }
  HSHM_INLINE_CROSS_FUN double GetSec() const { return time_ns_ / 1000000000; }
};

template <typename T>
class TimerBase : public TimepointBase<T>, public NsecTimer {
 public:
  /** Constructor */
  HSHM_INLINE_CROSS_FUN
  TimerBase() {}

  /** Resume timer */
  HSHM_INLINE_CROSS_FUN void Resume() { TimepointBase<T>::Now(); }

  /** Pause timer */
  HSHM_INLINE_CROSS_FUN double Pause() {
    time_ns_ += TimepointBase<T>::GetNsecFromStart();
    return time_ns_;
  }

  /** Reset timer */
  HSHM_INLINE_CROSS_FUN void Reset() {
    Resume();
    time_ns_ = 0;
  }

  /** Get microseconds since timer started */
  HSHM_INLINE_CROSS_FUN double GetUsFromEpoch() const {
#if HSHM_IS_HOST
    std::chrono::time_point<std::chrono::system_clock> point =
        std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>(
               point.time_since_epoch())
        .count();
#else
    return 0;
#endif
  }
};

typedef TimerBase<std::chrono::high_resolution_clock> HighResCpuTimer;
typedef TimerBase<std::chrono::steady_clock> HighResMonotonicTimer;
typedef HighResMonotonicTimer Timer;
typedef TimepointBase<std::chrono::high_resolution_clock> HighResCpuTimepoint;
typedef TimepointBase<std::chrono::steady_clock> HighResMonotonicTimepoint;
typedef HighResMonotonicTimepoint Timepoint;

template <int IDX>
class PeriodicRun {
 public:
  HighResMonotonicTimer timer_;

  PeriodicRun() { timer_.Resume(); }

  template <typename LAMBDA>
  void Run(size_t max_nsec, LAMBDA &&lambda) {
    size_t nsec = timer_.GetNsecFromStart();
    if (nsec >= max_nsec) {
      lambda();
      timer_.Reset();
    }
  }
};

#define HSHM_PERIODIC(IDX) \
  hshm::CrossSingleton<hshm::PeriodicRun<IDX>>::GetInstance()

}  // namespace hshm

#endif  // HSHM_TIMER_H
