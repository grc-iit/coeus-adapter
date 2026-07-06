#ifndef RANKCONSENSUS_TASKS_H_
#define RANKCONSENSUS_TASKS_H_

#include <clio_runtime/clio_runtime.h>
// Admin tasks for GetOrCreatePoolTask / DestroyPoolTask
#include <clio_runtime/admin/admin_tasks.h>

#include "autogen/rankConsensus_methods.h"

/**
 * Task struct definitions for rankConsensus ChiMod (clio-core port).
 *
 * Defines the tasks for Create and GetRank methods.
 */

namespace coeus::rankConsensus {

/**
 * CreateParams for rankConsensus chimod
 */
struct CreateParams {
  // Required: chimod library name for module manager
  static constexpr const char* chimod_lib_name = "coeus_rankConsensus";

  // Default constructor
  CreateParams() = default;

  // Serialization support for cereal
  template <class Archive>
  void serialize(Archive& ar) {
    // No additional fields to serialize
    (void)ar;
  }

  /**
   * Load configuration from PoolConfig (for compose mode)
   */
  void LoadConfig(const clio::run::PoolConfig& pool_config) {
    (void)pool_config;
  }
};

/**
 * CreateTask - Initialize the rankConsensus container
 */
using CreateTask = clio::run::admin::GetOrCreatePoolTask<CreateParams>;

/**
 * DestroyTask - Destroy the rankConsensus container
 */
using DestroyTask = clio::run::admin::DestroyPoolTask;

/**
 * GetRankTask - Get rank assignment
 */
struct GetRankTask : public clio::run::Task {
  // Output result
  OUT clio::run::u32 rank_;

  /** SHM default constructor */
  CTP_CROSS_FUN GetRankTask() : clio::run::Task(), rank_(0) {}

  /** Emplace constructor */
  explicit GetRankTask(const clio::run::TaskId& task_node,
                       const clio::run::PoolId& pool_id,
                       const clio::run::PoolQuery& pool_query)
      : clio::run::Task(task_node, pool_id, pool_query, Method::kGetRank),
        rank_(0) {
    // Initialize task
    task_id_ = task_node;
    pool_id_ = pool_id;
    method_ = Method::kGetRank;
    task_flags_.Clear();
    pool_query_ = pool_query;
  }

  /**
   * Serialize IN and INOUT parameters for network transfer
   * No input parameters for GetRankTask
   */
  template <typename Archive>
  CTP_CROSS_FUN void SerializeIn(Archive& ar) {
    Task::SerializeIn(ar);
    // No input parameters
  }

  /**
   * Serialize OUT and INOUT parameters for network transfer
   * This includes: rank_
   */
  template <typename Archive>
  CTP_CROSS_FUN void SerializeOut(Archive& ar) {
    Task::SerializeOut(ar);
    ar(rank_);
  }

  /**
   * Copy from another GetRankTask (assumes this task is already constructed)
   */
  void Copy(const ctp::ipc::FullPtr<GetRankTask>& other) {
    Task::Copy(other.template Cast<Task>());
    rank_ = other->rank_;
  }

  /**
   * Aggregate replica results into this task
   */
  void AggregateOut(const ctp::ipc::FullPtr<clio::run::Task>& other_base) {
    Task::AggregateOut(other_base);
    Copy(other_base.template Cast<GetRankTask>());
  }
};

}  // namespace coeus::rankConsensus

#endif  // RANKCONSENSUS_TASKS_H_
