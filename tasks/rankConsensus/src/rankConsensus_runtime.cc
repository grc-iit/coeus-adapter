/**
 * Runtime implementation for rankConsensus
 *
 * Contains the server-side task processing logic.
 */

#include "../include/chimaera/rankConsensus/rankConsensus_runtime.h"

namespace chimaera::rankConsensus {

// Virtual method implementations (Init, Run, DelTask, SaveTask, LoadTask, NewCopy, Aggregate) 
// are in autogen/rankConsensus_lib_exec.cc

//===========================================================================
// Method implementations
//===========================================================================

void Runtime::Create(hipc::FullPtr<CreateTask> task, chi::RunContext& rctx) {
  HLOG(kDebug, "rankConsensus: Executing Create task for pool {}", task->pool_id_);

  // Initialize rank counter
  rank_count_ = 0;

  HLOG(kDebug, "rankConsensus: Container created and initialized for pool: {} (ID: {})",
        pool_name_, task->pool_id_);
}

void Runtime::GetRank(hipc::FullPtr<GetRankTask> task, chi::RunContext& rctx) {
  HLOG(kDebug, "rankConsensus: Executing GetRank task");

  // Assign rank by incrementing atomic counter
  task->rank_ = rank_count_.fetch_add(1);

  task->SetReturnCode(0);  // Success
  HLOG(kDebug, "rankConsensus: GetRank completed, assigned rank: {}", task->rank_);
}

void Runtime::Destroy(hipc::FullPtr<DestroyTask> task, chi::RunContext& rctx) {
  HLOG(kDebug, "rankConsensus: Executing Destroy task - Pool ID: {}",
        task->target_pool_id_);

  // Reset rank counter
  rank_count_ = 0;

  // Initialize output values
  task->return_code_ = 0;
  task->error_message_ = "";

  HLOG(kDebug, "rankConsensus: Container destroyed successfully");
}

chi::u64 Runtime::GetWorkRemaining() const {
  // Return 0 as rank assignment is typically fast
  return 0;
}

}  // namespace chimaera::rankConsensus

