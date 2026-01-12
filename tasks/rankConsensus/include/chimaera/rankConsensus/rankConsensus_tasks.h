#ifndef RANKCONSENSUS_TASKS_H_
#define RANKCONSENSUS_TASKS_H_

#include <chimaera/chimaera.h>
#include "autogen/rankConsensus_methods.h"
// Include admin tasks for GetOrCreatePoolTask
#include <chimaera/admin/admin_tasks.h>

/**
 * Task struct definitions for rankConsensus ChiMod
 * 
 * Defines the tasks for Create and GetRank methods.
 */

namespace chimaera::rankConsensus {

/**
 * CreateParams for rankConsensus chimod
 * Contains configuration parameters for rankConsensus container creation
 */
struct CreateParams {
  // rankConsensus doesn't need additional parameters beyond base ones

  // Required: chimod library name for module manager
  static constexpr const char* chimod_lib_name = "chimaera_rankConsensus";

  // Default constructor
  CreateParams() = default;

  // Serialization support for cereal
  template<class Archive>
  void serialize(Archive& ar) {
    // No additional fields to serialize
    (void)ar; // Suppress unused parameter warning
  }

  /**
   * Load configuration from PoolConfig (for compose mode)
   * @param pool_config Pool configuration from compose section
   */
  void LoadConfig(const chi::PoolConfig &pool_config) {
    // rankConsensus doesn't have additional configuration fields
    (void)pool_config; // Suppress unused parameter warning
  }
};

/**
 * CreateTask - Initialize the rankConsensus container
 * Type alias for GetOrCreatePoolTask with CreateParams (uses kGetOrCreatePool method)
 */
using CreateTask = chimaera::admin::GetOrCreatePoolTask<CreateParams>;

/**
 * DestroyTask - Destroy the rankConsensus container
 * Type alias for DestroyPoolTask from admin namespace
 */
using DestroyTask = chimaera::admin::DestroyPoolTask;

/**
 * GetRankTask - Get rank assignment
 */
struct GetRankTask : public chi::Task {
  // Output result
  OUT chi::u32 rank_;

  /** SHM default constructor */
  GetRankTask()
      : chi::Task(),
        rank_(0) {}

  /** Emplace constructor */
  explicit GetRankTask(
      const chi::TaskId &task_node,
      const chi::PoolId &pool_id,
      const chi::PoolQuery &pool_query)
      : chi::Task(task_node, pool_id, pool_query, Method::kGetRank),
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
  template<typename Archive>
  void SerializeIn(Archive& ar) {
    Task::SerializeIn(ar);
    // No input parameters
  }

  /**
   * Serialize OUT and INOUT parameters for network transfer
   * This includes: rank_
   */
  template<typename Archive>
  void SerializeOut(Archive& ar) {
    Task::SerializeOut(ar);
    ar(rank_);
  }

  /**
   * Copy from another GetRankTask (assumes this task is already constructed)
   * @param other Pointer to the source task to copy from
   */
  void Copy(const hipc::FullPtr<GetRankTask> &other) {
    // Copy base Task fields
    Task::Copy(other.template Cast<Task>());
    // Copy GetRankTask-specific fields
    rank_ = other->rank_;
  }

  /**
   * Aggregate replica results into this task
   * @param other Pointer to the replica task to aggregate from
   */
  void Aggregate(const hipc::FullPtr<GetRankTask> &other) {
    Task::Aggregate(other.template Cast<Task>());
    Copy(other);
  }
};

}  // namespace chimaera::rankConsensus

#endif  // RANKCONSENSUS_TASKS_H_

