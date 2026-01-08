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

#ifndef HSHM_SHM_INCLUDE_HSHM_SHM_UTIL_TIMER_MPI_H_
#define HSHM_SHM_INCLUDE_HSHM_SHM_UTIL_TIMER_MPI_H_

#if HSHM_ENABLE_MPI

#include <mpi.h>

#include "timer.h"

namespace hshm {

class MpiTimer : public Timer {
 public:
  MPI_Comm comm_;
  int rank_;
  int nprocs_;

 public:
  explicit MpiTimer(MPI_Comm comm) : comm_(comm) {
    MPI_Comm_rank(comm_, &rank_);
    MPI_Comm_size(comm_, &nprocs_);
  }

  MpiTimer& Collect() { return CollectAvg(); }

  MpiTimer& CollectMax() {
    MPI_Barrier(comm_);
    double my_nsec = GetNsec();
    MPI_Allreduce(&my_nsec, &time_ns_, 1, MPI_DOUBLE, MPI_MAX, comm_);
    return *this;
  }

  MpiTimer& CollectMin() {
    MPI_Barrier(comm_);
    double my_nsec = GetNsec();
    MPI_Allreduce(&my_nsec, &time_ns_, 1, MPI_DOUBLE, MPI_MIN, comm_);
    return *this;
  }

  MpiTimer& CollectAvg() {
    MPI_Barrier(comm_);
    double my_nsec = GetNsec();
    double total_time_ns;
    MPI_Allreduce(&my_nsec, &total_time_ns, 1, MPI_DOUBLE, MPI_SUM, comm_);
    time_ns_ = total_time_ns / nprocs_;
    return *this;
  }
};

}  // namespace hshm
#endif  // HSHM_ENABLE_MPI
#endif  // HSHM_SHM_INCLUDE_HSHM_SHM_UTIL_TIMER_MPI_H_
