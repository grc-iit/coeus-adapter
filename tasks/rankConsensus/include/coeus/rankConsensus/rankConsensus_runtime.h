#ifndef RANKCONSENSUS_RUNTIME_H_
#define RANKCONSENSUS_RUNTIME_H_

#include <clio_runtime/clio_runtime.h>
#include <clio_runtime/container.h>

#include "autogen/rankConsensus_methods.h"
#include "rankConsensus_client.h"
#include "rankConsensus_tasks.h"

#include <atomic>

namespace coeus::rankConsensus {

/**
 * Runtime implementation for rankConsensus container (clio-core port).
 */
class Runtime : public clio::run::Container {
 public:
  // CreateParams type used by CLIO_CHIMOD_CC / module manager for lib_name
  using CreateParams = coeus::rankConsensus::CreateParams;

 private:
  // Container-specific state
  std::atomic<clio::run::u32> rank_count_;

  // Client for making calls to this ChiMod
  Client client_;

 public:
  Runtime() : rank_count_(0) {}
  virtual ~Runtime() = default;

  //===========================================================================
  // Container virtual API (implemented in autogen/rankConsensus_lib_exec.cc)
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
  // Method implementations (rankConsensus_runtime.cc)
  //===========================================================================

  clio::run::TaskResume Create(clio::run::shared_ptr<CreateTask>& task);
  clio::run::TaskResume GetRank(clio::run::shared_ptr<GetRankTask>& task);
  clio::run::TaskResume Destroy(clio::run::shared_ptr<DestroyTask>& task);
};

}  // namespace coeus::rankConsensus

#endif  // RANKCONSENSUS_RUNTIME_H_
