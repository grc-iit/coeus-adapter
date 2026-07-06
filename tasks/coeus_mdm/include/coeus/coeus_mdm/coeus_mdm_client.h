#ifndef COEUS_MDM_CLIENT_H_
#define COEUS_MDM_CLIENT_H_

#include <clio_runtime/clio_runtime.h>

#include "coeus_mdm_tasks.h"

/**
 * Client API for coeus_mdm ChiMod (clio-core port).
 *
 * Provides async methods for external programs to submit metadata operations.
 * All methods return Future objects - call Wait() to block for completion.
 */

namespace coeus::coeus_mdm {

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
                                            const clio::run::PoolId& custom_pool_id,
                                            const std::string& db_path) {
    auto* ipc_manager = CLIO_IPC;

    // CreateTask is a GetOrCreatePoolTask, handled by the admin pool.
    // Pass 'this' as client pointer for PostWait callback.
    auto task = ipc_manager->NewTask<CreateTask>(
        clio::run::CreateTaskId(),
        clio::run::kAdminPoolId,
        pool_query,
        CreateParams::chimod_lib_name,
        pool_name,
        custom_pool_id,
        this,
        db_path);

    return ipc_manager->Send(task);
  }

  /**
   * Synchronous Create wrapper
   */
  void Create(const clio::run::PoolQuery& pool_query,
              const std::string& pool_name,
              const clio::run::PoolId& custom_pool_id,
              const std::string& db_path) {
    auto future = AsyncCreate(pool_query, pool_name, custom_pool_id, db_path);
    future.Wait();
    if (future->GetReturnCode() != 0) {
      HLOG(kError, "coeus_mdm::Create failed with return code: {}",
           future->GetReturnCode());
      return;
    }
    // Update client pool_id_ with the actual pool ID from the task
    pool_id_ = future->new_pool_id_;
  }

  /**
   * Insert metadata operation (asynchronous)
   */
  clio::run::Future<Mdm_insertTask> AsyncMdm_insert(const clio::run::PoolQuery& pool_query,
                                                    const DbOperation& db_op) {
    auto* ipc_manager = CLIO_IPC;

    auto task = ipc_manager->NewTask<Mdm_insertTask>(
        clio::run::CreateTaskId(), pool_id_, pool_query, db_op);

    return ipc_manager->Send(task);
  }

  /**
   * Synchronous Mdm_insert wrapper
   */
  void Mdm_insert(const clio::run::PoolQuery& pool_query, const DbOperation& db_op) {
    auto future = AsyncMdm_insert(pool_query, db_op);
    future.Wait();
    if (future->GetReturnCode() != 0) {
      HLOG(kError, "coeus_mdm::Mdm_insert failed with return code: {}",
           future->GetReturnCode());
    }
  }
};

}  // namespace coeus::coeus_mdm

#endif  // COEUS_MDM_CLIENT_H_
