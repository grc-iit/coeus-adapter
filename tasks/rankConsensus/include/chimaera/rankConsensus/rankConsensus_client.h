#ifndef RANKCONSENSUS_CLIENT_H_
#define RANKCONSENSUS_CLIENT_H_

#include <chimaera/chimaera.h>

#include "rankConsensus_tasks.h"

/**
 * Client API for rankConsensus ChiMod
 *
 * Provides async methods for external programs to get rank assignments.
 * All methods return Future objects - call Wait() to block for completion.
 */

namespace chimaera::rankConsensus {

class Client : public chi::ContainerClient {
 public:
  /** Default constructor */
  Client() = default;

  /** Constructor with pool ID */
  explicit Client(const chi::PoolId& pool_id) { Init(pool_id); }

  /**
   * Create the container (asynchronous)
   * @param pool_query Pool routing information
   * @param pool_name Unique name for the pool (user-provided)
   * @param custom_pool_id Explicit pool ID for the pool being created
   * @return Future for the CreateTask
   */
  chi::Future<CreateTask> AsyncCreate(const chi::PoolQuery& pool_query,
                                       const std::string& pool_name,
                                       const chi::PoolId& custom_pool_id) {
    auto* ipc_manager = CHI_IPC;

    // CreateTask is a GetOrCreatePoolTask, which must be handled by admin pool
    // Pass 'this' as client pointer for PostWait callback
    auto task = ipc_manager->NewTask<CreateTask>(
        chi::CreateTaskId(),
        chi::kAdminPoolId,  // Send to admin pool for GetOrCreatePool processing
        pool_query,
        CreateParams::chimod_lib_name,  // chimod name from CreateParams
        pool_name,                      // user-provided pool name
        custom_pool_id,                 // target pool ID to create
        this                            // Client pointer for PostWait
    );

    return ipc_manager->Send(task);
  }

  /**
   * Synchronous Create wrapper
   * @param pool_query Pool routing information
   * @param pool_name Unique name for the pool
   * @param custom_pool_id Explicit pool ID for the pool being created
   */
  void Create(const chi::PoolQuery& pool_query,
              const std::string& pool_name,
              const chi::PoolId& custom_pool_id) {
    auto future = AsyncCreate(pool_query, pool_name, custom_pool_id);
    future.Wait();
    if (future->GetReturnCode() != 0) {
      HLOG(kError, "rankConsensus::Create failed with return code: {}",
            future->GetReturnCode());
      return; // Early return on error
    }
    // CRITICAL: Update client pool_id_ with the actual pool ID from the task
    // This is required because the returned pool ID may differ from the requested ID
    pool_id_ = future->new_pool_id_;
  }

  /**
   * Get rank assignment (asynchronous)
   * @param pool_query Pool routing information
   * @return Future for the GetRankTask
   */
  chi::Future<GetRankTask> AsyncGetRank(const chi::PoolQuery& pool_query) {
    auto* ipc_manager = CHI_IPC;

    auto task = ipc_manager->NewTask<GetRankTask>(
        chi::CreateTaskId(), pool_id_, pool_query);

    return ipc_manager->Send(task);
  }

  /**
   * Synchronous GetRank wrapper
   * @param pool_query Pool routing information
   * @return Assigned rank number
   */
  chi::u32 GetRank(const chi::PoolQuery& pool_query) {
    auto future = AsyncGetRank(pool_query);
    future.Wait();
    if (future->GetReturnCode() != 0) {
      HLOG(kError, "rankConsensus::GetRank failed with return code: {}",
            future->GetReturnCode());
      return 0; // Return 0 on error
    }
    return future->rank_;
  }
};

}  // namespace chimaera::rankConsensus

#endif  // RANKCONSENSUS_CLIENT_H_

