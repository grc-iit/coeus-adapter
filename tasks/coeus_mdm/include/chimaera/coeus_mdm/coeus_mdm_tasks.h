#ifndef COEUS_MDM_TASKS_H_
#define COEUS_MDM_TASKS_H_

#include <chimaera/chimaera.h>
#include "autogen/coeus_mdm_methods.h"
// Include admin tasks for GetOrCreatePoolTask
#include <chimaera/admin/admin_tasks.h>

#include "common/DbOperation.h"

/**
 * Task struct definitions for coeus_mdm ChiMod
 * 
 * Defines the tasks for Create and Mdm_insert methods.
 */

namespace chimaera::coeus_mdm {

/**
 * CreateParams for coeus_mdm chimod
 * Contains configuration parameters for coeus_mdm container creation
 */
struct CreateParams {
  // coeus_mdm-specific parameters
  std::string db_path_;

  // Required: chimod library name for module manager
  static constexpr const char* chimod_lib_name = "chimaera_coeus_mdm";

  // Default constructor
  CreateParams() = default;

  // Constructor with parameters
  CreateParams(const std::string& db_path) : db_path_(db_path) {}

  // Serialization support for cereal
  template<class Archive>
  void serialize(Archive& ar) {
    ar(db_path_);
  }

  /**
   * Load configuration from PoolConfig (for compose mode)
   * @param pool_config Pool configuration from compose section
   */
  void LoadConfig(const chi::PoolConfig &pool_config) {
    // coeus_mdm doesn't have additional configuration fields in PoolConfig
    // db_path would be set via constructor
    (void)pool_config; // Suppress unused parameter warning
  }
};

/**
 * CreateTask - Initialize the coeus_mdm container
 * Type alias for GetOrCreatePoolTask with CreateParams (uses kGetOrCreatePool method)
 */
using CreateTask = chimaera::admin::GetOrCreatePoolTask<CreateParams>;

/**
 * DestroyTask - Destroy the coeus_mdm container
 * Type alias for DestroyPoolTask from admin namespace
 */
using DestroyTask = chimaera::admin::DestroyPoolTask;

/**
 * Mdm_insertTask - Insert metadata operation
 */
struct Mdm_insertTask : public chi::Task {
  // Task-specific data
  IN chi::priv::string db_op_serialized_;  // Serialized DbOperation

  /** SHM default constructor */
  Mdm_insertTask()
      : chi::Task(),
        db_op_serialized_(HSHM_MALLOC) {}

  /** Emplace constructor */
  explicit Mdm_insertTask(
      const chi::TaskId &task_node,
      const chi::PoolId &pool_id,
      const chi::PoolQuery &pool_query,
      const DbOperation &db_op)
      : chi::Task(task_node, pool_id, pool_query, Method::kMdm_insert),
        db_op_serialized_(HSHM_MALLOC) {
    // Initialize task
    task_id_ = task_node;
    pool_id_ = pool_id;
    method_ = Method::kMdm_insert;
    task_flags_.Clear();
    pool_query_ = pool_query;

    // Serialize DbOperation
    chi::Task::Serialize(HSHM_MALLOC, db_op_serialized_, db_op);
  }

  /**
   * Get the DbOperation by deserializing from db_op_serialized_
   */
  DbOperation GetDbOp() const {
    return chi::Task::Deserialize<DbOperation>(db_op_serialized_);
  }

  /**
   * Serialize IN and INOUT parameters for network transfer
   * This includes: db_op_serialized_
   */
  template<typename Archive>
  void SerializeIn(Archive& ar) {
    Task::SerializeIn(ar);
    ar(db_op_serialized_);
  }

  /**
   * Serialize OUT and INOUT parameters for network transfer
   * No output parameters for Mdm_insertTask
   */
  template<typename Archive>
  void SerializeOut(Archive& ar) {
    Task::SerializeOut(ar);
    // No output parameters
  }

  /**
   * Copy from another Mdm_insertTask (assumes this task is already constructed)
   * @param other Pointer to the source task to copy from
   */
  void Copy(const hipc::FullPtr<Mdm_insertTask> &other) {
    // Copy base Task fields
    Task::Copy(other.template Cast<Task>());
    // Copy Mdm_insertTask-specific fields
    db_op_serialized_ = other->db_op_serialized_;
  }

  /**
   * Aggregate replica results into this task
   * @param other Pointer to the replica task to aggregate from
   */
  void Aggregate(const hipc::FullPtr<Mdm_insertTask> &other) {
    Task::Aggregate(other.template Cast<Task>());
    Copy(other);
  }
};

}  // namespace chimaera::coeus_mdm

#endif  // COEUS_MDM_TASKS_H_

