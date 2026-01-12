#ifndef COEUS_MDM_RUNTIME_H_
#define COEUS_MDM_RUNTIME_H_

#include <chimaera/chimaera.h>
#include <chimaera/container.h>
#include "coeus_mdm_tasks.h"
#include "autogen/coeus_mdm_methods.h"
#include "coeus_mdm_client.h"

#include "common/SQlite.h"

namespace chimaera::coeus_mdm {

// Forward declarations
// Note: CreateTask is a type alias (not a struct), so no forward declaration needed
// Mdm_insertTask is fully defined in coeus_mdm_tasks.h which is included above

/**
 * Runtime implementation for coeus_mdm container
 */
class Runtime : public chi::Container {
 public:
  // CreateParams type used by CHI_TASK_CC macro for lib_name access
  using CreateParams = chimaera::coeus_mdm::CreateParams;

 private:
  // Container-specific state
  std::unique_ptr<SQLiteWrapper> db_;
  std::string db_path_;

  // Client for making calls to this ChiMod
  Client client_;

 public:
  /**
   * Constructor
   */
  Runtime() = default;

  /**
   * Destructor
   */
  virtual ~Runtime() = default;

  /**
   * Initialize container with pool information
   */
  void Init(const chi::PoolId& pool_id, const std::string& pool_name,
            chi::u32 container_id = 0) override;

  /**
   * Execute a method on a task
   */
  chi::TaskResume Run(chi::u32 method, hipc::FullPtr<chi::Task> task_ptr, chi::RunContext& rctx) override;

  /**
   * Delete/cleanup a task
   */
  void DelTask(chi::u32 method, hipc::FullPtr<chi::Task> task_ptr) override;

  /**
   * Serialize task parameters for network transfer
   */
  void SaveTask(chi::u32 method, chi::SaveTaskArchive& archive,
                hipc::FullPtr<chi::Task> task_ptr) override;

  /**
   * Deserialize task parameters into an existing task from network transfer
   */
  void LoadTask(chi::u32 method, chi::LoadTaskArchive& archive,
                hipc::FullPtr<chi::Task> task_ptr) override;

  /**
   * Allocate and deserialize task parameters from network transfer
   */
  hipc::FullPtr<chi::Task> AllocLoadTask(chi::u32 method, chi::LoadTaskArchive& archive) override;

  /**
   * Deserialize task input parameters into an existing task using LocalSerialize
   */
  void LocalLoadTask(chi::u32 method, chi::LocalLoadTaskArchive& archive,
                     hipc::FullPtr<chi::Task> task_ptr) override;

  /**
   * Allocate and deserialize task input parameters using LocalSerialize
   */
  hipc::FullPtr<chi::Task> LocalAllocLoadTask(chi::u32 method, chi::LocalLoadTaskArchive& archive) override;

  /**
   * Serialize task output parameters using LocalSerialize (for local transfers)
   */
  void LocalSaveTask(chi::u32 method, chi::LocalSaveTaskArchive& archive,
                     hipc::FullPtr<chi::Task> task_ptr) override;

  /**
   * Create a new copy of a task (deep copy for distributed execution)
   */
  hipc::FullPtr<chi::Task> NewCopyTask(chi::u32 method, hipc::FullPtr<chi::Task> orig_task_ptr, bool deep) override;

  /**
   * Create a new task of the specified method type
   */
  hipc::FullPtr<chi::Task> NewTask(chi::u32 method) override;

  /**
   * Aggregate replica results into origin task
   */
  void Aggregate(chi::u32 method, hipc::FullPtr<chi::Task> origin_task_ptr,
                 hipc::FullPtr<chi::Task> replica_task_ptr) override;

  //===========================================================================
  // Method implementations
  //===========================================================================

  /**
   * Handle Create task
   */
  void Create(hipc::FullPtr<CreateTask> task, chi::RunContext& rctx);

  /**
   * Handle Mdm_insert task
   */
  void Mdm_insert(hipc::FullPtr<Mdm_insertTask> task, chi::RunContext& rctx);

  /**
   * Handle Destroy task
   */
  void Destroy(hipc::FullPtr<DestroyTask> task, chi::RunContext& rctx);

  /**
   * Get remaining work count
   */
  chi::u64 GetWorkRemaining() const override;
};

}  // namespace chimaera::coeus_mdm

#endif  // COEUS_MDM_RUNTIME_H_

