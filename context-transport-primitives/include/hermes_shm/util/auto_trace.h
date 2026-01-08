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

#ifndef HSHM_INCLUDE_HSHM_UTIL_AUTO_TRACE_H_
#define HSHM_INCLUDE_HSHM_UTIL_AUTO_TRACE_H_

#include <iostream>

#include "formatter.h"
#include "logging.h"
#include "timer.h"

namespace hshm {

#define AUTO_TRACE(LOG_CODE) hshm::AutoTrace<LOG_CODE> _hshm_tracer_(__func__)
#define TIMER_START(NAME) _hshm_tracer_.StartTimer(NAME)
#define TIMER_END() _hshm_tracer_.EndTimer()

/** Trace function execution times */
template <int LOG_CODE>
class AutoTrace {
 private:
  HighResMonotonicTimer timer_;
  HighResMonotonicTimer timer2_;
  std::string fname_;
  std::string internal_name_;

 public:
  HSHM_INLINE AutoTrace(const char *fname) {
    if constexpr (LOG_CODE >= 0) {
      fname_ = fname;
      _StartTimer(timer_);
    }
  }

  HSHM_INLINE
  ~AutoTrace() {
    if constexpr (LOG_CODE >= 0) {
      _EndTimer(timer_);
    }
  }

  HSHM_INLINE
  void StartTimer(const char *internal_name) {
    if constexpr (LOG_CODE >= 0) {
      internal_name_ = "/" + std::string(internal_name);
      _StartTimer(timer2_);
    }
  }

  HSHM_INLINE
  void EndTimer() {
    if constexpr (LOG_CODE >= 0) {
      _EndTimer(timer2_);
    }
  }

 private:
  template <typename... Args>
  HSHM_INLINE void _StartTimer(HighResMonotonicTimer &timer) {
    timer.Resume();
    HIPRINT("{}{}", fname_, internal_name_);
  }

  HSHM_INLINE
  void _EndTimer(HighResMonotonicTimer &timer) {
    timer.Pause();
    HIPRINT("{}{} {}ns", fname_, internal_name_, timer.GetNsec());
    timer.Reset();
    internal_name_.clear();
  }
};

}  // namespace hshm

#endif  // HSHM_INCLUDE_HSHM_UTIL_AUTO_TRACE_H_
