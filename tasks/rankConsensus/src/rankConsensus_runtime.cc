/**
 * Runtime implementation for rankConsensus (clio-core port).
 *
 * Contains the server-side task processing logic. The Container virtual
 * dispatch API (Init, Run, SaveTask, LoadTask, NewTask, ...) is in
 * autogen/rankConsensus_lib_exec.cc.
 */

#include "coeus/rankConsensus/rankConsensus_runtime.h"

namespace coeus::rankConsensus {

//===========================================================================
// Method implementations
//===========================================================================

clio::run::TaskResume Runtime::Create(clio::run::shared_ptr<CreateTask>& task) {
  CLIO_TASK_BODY_BEGIN
  HLOG(kDebug, "rankConsensus: Executing Create task for pool {}", task->pool_id_);

  // Initialize rank counter
  rank_count_ = 0;

  task->return_code_ = 0;
  HLOG(kDebug, "rankConsensus: Container created for pool: {}", pool_name_);
  CLIO_CO_RETURN;
  CLIO_TASK_BODY_END
}

clio::run::TaskResume Runtime::GetRank(clio::run::shared_ptr<GetRankTask>& task) {
  CLIO_TASK_BODY_BEGIN
  HLOG(kDebug, "rankConsensus: Executing GetRank task");

  // Assign rank by incrementing atomic counter
  task->rank_ = rank_count_.fetch_add(1);

  task->SetReturnCode(0);  // Success
  HLOG(kDebug, "rankConsensus: GetRank completed, assigned rank: {}", task->rank_);
  CLIO_CO_RETURN;
  CLIO_TASK_BODY_END
}

clio::run::TaskResume Runtime::Destroy(clio::run::shared_ptr<DestroyTask>& task) {
  CLIO_TASK_BODY_BEGIN
  HLOG(kDebug, "rankConsensus: Executing Destroy task - Pool ID: {}",
       task->target_pool_id_);

  // Reset rank counter
  rank_count_ = 0;

  task->return_code_ = 0;
  task->error_message_ = clio::run::priv::string(CLIO_PRIV_ALLOC, "");

  HLOG(kDebug, "rankConsensus: Container destroyed successfully");
  CLIO_CO_RETURN;
  CLIO_TASK_BODY_END
}

clio::run::u64 Runtime::GetWorkRemaining() const {
  // Rank assignment is fast; no long-running work remaining.
  return 0;
}

}  // namespace coeus::rankConsensus

// Generate ChiMod entry points (alloc_chimod, get_chimod_name, etc.)
CLIO_CHIMOD_CC(coeus::rankConsensus::Runtime, "coeus_rankConsensus")
