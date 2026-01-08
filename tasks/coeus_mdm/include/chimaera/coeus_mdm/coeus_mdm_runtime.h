#ifndef COEUS_MDM_RUNTIME_H_
#define COEUS_MDM_RUNTIME_H_

#include <chimaera/chimaera.h>
#include <chimaera/container.h>
#include "coeus_mdm_tasks.h"
#include "autogen/coeus_mdm_methods.h"
#include "coeus_mdm_client.h"

#include "../../../../include/common/SQlite.h"

namespace chimaera::coeus_mdm {

// Forward declarations
struct CreateTask;
struct Mdm_insertTask;

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

