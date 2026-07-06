#ifndef COEUS_MDM_TASKS_H_
#define COEUS_MDM_TASKS_H_

#include <clio_runtime/clio_runtime.h>
// Admin tasks for GetOrCreatePoolTask / DestroyPoolTask
#include <clio_runtime/admin/admin_tasks.h>

#include "autogen/coeus_mdm_methods.h"
#include "common/DbOperation.h"

/**
 * Task struct definitions for coeus_mdm ChiMod (clio-core port).
 *
 * Defines the tasks for Create and Mdm_insert methods.
 */

namespace coeus::coeus_mdm {

/**
 * CreateParams for coeus_mdm chimod
 * Contains configuration parameters for coeus_mdm container creation
 */
struct CreateParams {
  // coeus_mdm-specific parameters
  std::string db_path_;

  // Required: chimod library name for module manager
  static constexpr const char* chimod_lib_name = "coeus_coeus_mdm";

  // Default constructor
  CreateParams() = default;

  // Constructor with parameters
  explicit CreateParams(const std::string& db_path) : db_path_(db_path) {}

  // Serialization support for cereal
  template <class Archive>
  void serialize(Archive& ar) {
    ar(db_path_);
  }

  /**
   * Load configuration from PoolConfig (for compose mode)
   */
  void LoadConfig(const clio::run::PoolConfig& pool_config) {
    // coeus_mdm has no additional PoolConfig fields; db_path is set via ctor
    (void)pool_config;
  }
};

/**
 * CreateTask - Initialize the coeus_mdm container
 * Type alias for GetOrCreatePoolTask with CreateParams
 */
using CreateTask = clio::run::admin::GetOrCreatePoolTask<CreateParams>;

/**
 * DestroyTask - Destroy the coeus_mdm container
 * Type alias for DestroyPoolTask from admin namespace
 */
using DestroyTask = clio::run::admin::DestroyPoolTask;

/**
 * Mdm_insertTask - Insert metadata operation
 */
struct Mdm_insertTask : public clio::run::Task {
  // Task-specific data
  IN clio::run::priv::string db_op_serialized_;  // Serialized DbOperation

  /** SHM default constructor */
  CTP_CROSS_FUN Mdm_insertTask()
      : clio::run::Task(), db_op_serialized_(CLIO_PRIV_ALLOC) {}

  /** Emplace constructor */
  explicit Mdm_insertTask(const clio::run::TaskId& task_node,
                          const clio::run::PoolId& pool_id,
                          const clio::run::PoolQuery& pool_query,
                          const DbOperation& db_op)
      : clio::run::Task(task_node, pool_id, pool_query, Method::kMdm_insert),
        db_op_serialized_(CLIO_PRIV_ALLOC) {
    // Initialize task
    task_id_ = task_node;
    pool_id_ = pool_id;
    method_ = Method::kMdm_insert;
    task_flags_.Clear();
    pool_query_ = pool_query;

    // Serialize DbOperation into the private-memory string
    clio::run::Task::Serialize(CLIO_PRIV_ALLOC, db_op_serialized_, db_op);
  }

  /**
   * Get the DbOperation by deserializing from db_op_serialized_
   */
  DbOperation GetDbOp() const {
    return clio::run::Task::Deserialize<DbOperation>(db_op_serialized_);
  }

  /**
   * Serialize IN and INOUT parameters for network transfer
   * This includes: db_op_serialized_
   */
  template <typename Archive>
  CTP_CROSS_FUN void SerializeIn(Archive& ar) {
    Task::SerializeIn(ar);
    ar(db_op_serialized_);
  }

  /**
   * Serialize OUT and INOUT parameters for network transfer
   * No output parameters for Mdm_insertTask
   */
  template <typename Archive>
  CTP_CROSS_FUN void SerializeOut(Archive& ar) {
    Task::SerializeOut(ar);
    // No output parameters
  }

  /**
   * Copy from another Mdm_insertTask (assumes this task is already constructed)
   */
  void Copy(const ctp::ipc::FullPtr<Mdm_insertTask>& other) {
    Task::Copy(other.template Cast<Task>());
    db_op_serialized_ = other->db_op_serialized_;
  }

  /**
   * Aggregate replica results into this task
   */
  void AggregateOut(const ctp::ipc::FullPtr<clio::run::Task>& other_base) {
    Task::AggregateOut(other_base);
    Copy(other_base.template Cast<Mdm_insertTask>());
  }
};

}  // namespace coeus::coeus_mdm

#endif  // COEUS_MDM_TASKS_H_
