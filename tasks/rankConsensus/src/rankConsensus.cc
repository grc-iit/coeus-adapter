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

#include "hrun_admin/hrun_admin.h"
#include "hrun/api/hrun_runtime.h"
#include "rankConsensus/rankConsensus.h"

namespace hrun::rankConsensus {

class Server : public TaskLib {
 public:
  Server() = default;
  std::atomic<uint> rank_count;
  /** Construct rankConsensus */
  void Construct(ConstructTask *task, RunContext &rctx) {
    rank_count = 0;
    task->SetModuleComplete();
  }
  void MonitorConstruct(u32 mode, ConstructTask *task, RunContext &rctx) {
  }

  /** Destroy rankConsensus */
  void Destruct(DestructTask *task, RunContext &rctx) {
    task->SetModuleComplete();
  }
  void MonitorDestruct(u32 mode, DestructTask *task, RunContext &rctx) {
  }

  /** A custom method */
  void GetRank(GetRankTask *task, RunContext &rctx) {
    task->rank_ = rank_count.fetch_add(1);
    task->SetModuleComplete();
  }
  void MonitorGetRank(u32 mode, GetRankTask *task, RunContext &rctx) {
  }
 public:
#include "rankConsensus/rankConsensus_lib_exec.h"
};

}  // namespace hrun::rankConsensus

HRUN_TASK_CC(hrun::rankConsensus::Server, "rankConsensus");
