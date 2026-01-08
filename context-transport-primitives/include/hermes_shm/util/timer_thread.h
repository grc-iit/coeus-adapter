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

#ifndef HSHM_SHM_INCLUDE_HSHM_SHM_UTIL_TIMER_THREAD_H_
#define HSHM_SHM_INCLUDE_HSHM_SHM_UTIL_TIMER_THREAD_H_

#include "timer.h"

namespace hshm {

class ThreadTimer : public NsecTimer {
 public:
  int rank_;
  int nprocs_;
  std::vector<Timer> timers_;

 public:
  explicit ThreadTimer(int nthreads) {
    nprocs_ = nthreads;
    timers_.resize(nprocs_);
  }

  void SetRank(int rank) { rank_ = rank; }

  void Resume() { timers_[rank_].Resume(); }

  void Pause() { timers_[rank_].Pause(); }

  void Reset() { timers_[rank_].Reset(); }

  void Collect() {
    std::vector<double> rank_times;
    rank_times.reserve(nprocs_);
    for (Timer &t : timers_) {
      rank_times.push_back(t.GetNsec());
    }
    time_ns_ = *std::max_element(rank_times.begin(), rank_times.end());
  }
};

}  // namespace hshm

#endif  // HSHM_SHM_INCLUDE_HSHM_SHM_UTIL_TIMER_THREAD_H_
