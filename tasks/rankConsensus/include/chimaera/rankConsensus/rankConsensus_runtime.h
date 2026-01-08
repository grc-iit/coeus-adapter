#ifndef RANKCONSENSUS_RUNTIME_H_
#define RANKCONSENSUS_RUNTIME_H_

#include <chimaera/chimaera.h>
#include <chimaera/container.h>
#include "rankConsensus_tasks.h"
#include "autogen/rankConsensus_methods.h"
#include "rankConsensus_client.h"

#include <atomic>

namespace chimaera::rankConsensus {

// Forward declarations
struct CreateTask;
struct GetRankTask;

/**
 * Runtime implementation for rankConsensus container
 */
class Runtime : public chi::Container {
 public:
  // CreateParams type used by CHI_TASK_CC macro for lib_name access
  using CreateParams = chimaera::rankConsensus::CreateParams;

 private:
  // Container-specific state
  std::atomic<chi::u32> rank_count_;

  // Client for making calls to this ChiMod
  Client client_;

 public:
  /**
   * Constructor
   */
  Runtime() : rank_count_(0) {}

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
   * Handle GetRank task
   */
  void GetRank(hipc::FullPtr<GetRankTask> task, chi::RunContext& rctx);

  /**
   * Handle Destroy task
   */
  void Destroy(hipc::FullPtr<DestroyTask> task, chi::RunContext& rctx);

  /**
   * Get remaining work count
   */
  chi::u64 GetWorkRemaining() const override;
};

}  // namespace chimaera::rankConsensus

#endif  // RANKCONSENSUS_RUNTIME_H_

