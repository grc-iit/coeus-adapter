#ifndef COEUS_MDM_RUNTIME_H_
#define COEUS_MDM_RUNTIME_H_

#include <clio_runtime/clio_runtime.h>
#include <clio_runtime/container.h>

#include "autogen/coeus_mdm_methods.h"
#include "coeus_mdm_client.h"
#include "coeus_mdm_tasks.h"

#include "common/SQlite.h"

namespace coeus::coeus_mdm {

/**
 * Runtime implementation for coeus_mdm container (clio-core port).
 */
class Runtime : public clio::run::Container {
 public:
  // CreateParams type used by CLIO_CHIMOD_CC / module manager for lib_name
  using CreateParams = coeus::coeus_mdm::CreateParams;

 private:
  // Container-specific state
  std::unique_ptr<SQLiteWrapper> db_;
  std::string db_path_;

  // Client for making calls to this ChiMod
  Client client_;

 public:
  Runtime() = default;
  virtual ~Runtime() = default;

  //===========================================================================
  // Container virtual API (implemented in autogen/coeus_mdm_lib_exec.cc)
  //===========================================================================

  void Init(const clio::run::PoolId& pool_id, const std::string& pool_name,
            clio::run::u32 container_id = 0) override;

  clio::run::TaskResume Run(clio::run::u32 method,
                            clio::run::shared_ptr<clio::run::Task> task_ptr) override;

  void SaveTask(clio::run::u32 method, clio::run::SaveTaskArchive& archive,
                clio::run::shared_ptr<clio::run::Task>& task_ptr) override;

  void LoadTask(clio::run::u32 method, clio::run::LoadTaskArchive& archive,
                clio::run::shared_ptr<clio::run::Task>& task_ptr) override;

  clio::run::shared_ptr<clio::run::Task> AllocLoadTask(
      clio::run::u32 method, clio::run::LoadTaskArchive& archive) override;

  void LocalLoadTask(clio::run::u32 method, clio::run::DefaultLoadArchive& archive,
                     clio::run::shared_ptr<clio::run::Task>& task_ptr) override;

  clio::run::shared_ptr<clio::run::Task> LocalAllocLoadTask(
      clio::run::u32 method, clio::run::DefaultLoadArchive& archive) override;

  void LocalSaveTask(clio::run::u32 method, clio::run::DefaultSaveArchive& archive,
                     clio::run::shared_ptr<clio::run::Task>& task_ptr) override;

  clio::run::shared_ptr<clio::run::Task> NewCopyTask(
      clio::run::u32 method, clio::run::shared_ptr<clio::run::Task>& orig_task_ptr,
      bool deep) override;

  clio::run::shared_ptr<clio::run::Task> NewTask(clio::run::u32 method) override;

  void AggregateOut(clio::run::u32 method,
                    clio::run::shared_ptr<clio::run::Task>& orig_task,
                    const clio::run::shared_ptr<clio::run::Task>& replica_task) override;

  clio::run::u64 GetWorkRemaining() const override;

  //===========================================================================
  // Method implementations (coeus_mdm_runtime.cc)
  //===========================================================================

  clio::run::TaskResume Create(clio::run::shared_ptr<CreateTask>& task);
  clio::run::TaskResume Mdm_insert(clio::run::shared_ptr<Mdm_insertTask>& task);
  clio::run::TaskResume Destroy(clio::run::shared_ptr<DestroyTask>& task);
};

}  // namespace coeus::coeus_mdm

#endif  // COEUS_MDM_RUNTIME_H_
