#ifndef RANKCONSENSUS_CLIENT_H_
#define RANKCONSENSUS_CLIENT_H_

#include <clio_runtime/clio_runtime.h>

#include "rankConsensus_tasks.h"

/**
 * Client API for rankConsensus ChiMod (clio-core port).
 *
 * Provides async methods for external programs to get rank assignments.
 * All methods return Future objects - call Wait() to block for completion.
 */

namespace coeus::rankConsensus {

class Client : public clio::run::ContainerClient {
 public:
  /** Default constructor */
  Client() = default;

  /** Constructor with pool ID */
  explicit Client(const clio::run::PoolId& pool_id) { Init(pool_id); }

  /**
   * Create the container (asynchronous)
   */
  clio::run::Future<CreateTask> AsyncCreate(const clio::run::PoolQuery& pool_query,
                                            const std::string& pool_name,
                                            const clio::run::PoolId& custom_pool_id) {
    auto* ipc_manager = CLIO_IPC;

    // CreateTask is a GetOrCreatePoolTask, handled by the admin pool.
    auto task = ipc_manager->NewTask<CreateTask>(
        clio::run::CreateTaskId(),
        clio::run::kAdminPoolId,
        pool_query,
        CreateParams::chimod_lib_name,
        pool_name,
        custom_pool_id,
        this);

    return ipc_manager->Send(task);
  }

  /**
   * Synchronous Create wrapper
   */
  void Create(const clio::run::PoolQuery& pool_query,
              const std::string& pool_name,
              const clio::run::PoolId& custom_pool_id) {
    auto future = AsyncCreate(pool_query, pool_name, custom_pool_id);
    future.Wait();
    if (future->GetReturnCode() != 0) {
      HLOG(kError, "rankConsensus::Create failed with return code: {}",
           future->GetReturnCode());
      return;
    }
    // Update client pool_id_ with the actual pool ID from the task
    pool_id_ = future->new_pool_id_;
  }

  /**
   * Get rank assignment (asynchronous)
   */
  clio::run::Future<GetRankTask> AsyncGetRank(const clio::run::PoolQuery& pool_query) {
    auto* ipc_manager = CLIO_IPC;

    auto task = ipc_manager->NewTask<GetRankTask>(
        clio::run::CreateTaskId(), pool_id_, pool_query);

    return ipc_manager->Send(task);
  }

  /**
   * Synchronous GetRank wrapper
   */
  clio::run::u32 GetRank(const clio::run::PoolQuery& pool_query) {
    auto future = AsyncGetRank(pool_query);
    future.Wait();
    if (future->GetReturnCode() != 0) {
      HLOG(kError, "rankConsensus::GetRank failed with return code: {}",
           future->GetReturnCode());
      return 0;
    }
    return future->rank_;
  }
};

}  // namespace coeus::rankConsensus

#endif  // RANKCONSENSUS_CLIENT_H_
