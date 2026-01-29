#ifndef COEUS_MDM_CLIENT_H_
#define COEUS_MDM_CLIENT_H_

#include <chimaera/chimaera.h>

#include "coeus_mdm_tasks.h"

/**
 * Client API for coeus_mdm ChiMod
 *
 * Provides async methods for external programs to submit metadata operations.
 * All methods return Future objects - call Wait() to block for completion.
 */

namespace chimaera::coeus_mdm {

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
   * @param db_path Path to SQLite database file
   * @return Future for the CreateTask
   */
  chi::Future<CreateTask> AsyncCreate(const chi::PoolQuery& pool_query,
                                       const std::string& pool_name,
                                       const chi::PoolId& custom_pool_id,
                                       const std::string& db_path) {
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
        this,                           // Client pointer for PostWait
        db_path                         // CreateParams argument (db_path)
    );

    return ipc_manager->Send(task);
  }

  /**
   * Synchronous Create wrapper
   * @param pool_query Pool routing information
   * @param pool_name Unique name for the pool
   * @param custom_pool_id Explicit pool ID for the pool being created
   * @param db_path Path to SQLite database file
   */
  void Create(const chi::PoolQuery& pool_query,
              const std::string& pool_name,
              const chi::PoolId& custom_pool_id,
              const std::string& db_path) {
    auto future = AsyncCreate(pool_query, pool_name, custom_pool_id, db_path);
    future.Wait();
    if (future->GetReturnCode() != 0) {
      HLOG(kError, "coeus_mdm::Create failed with return code: {}",
            future->GetReturnCode());
      return; // Early return on error
    }
    // CRITICAL: Update client pool_id_ with the actual pool ID from the task
    // This is required because the returned pool ID may differ from the requested ID
    pool_id_ = future->new_pool_id_;
  }

  /**
   * Insert metadata operation (asynchronous)
   * @param pool_query Pool routing information
   * @param db_op Database operation to execute
   * @return Future for the Mdm_insertTask
   */
  chi::Future<Mdm_insertTask> AsyncMdm_insert(const chi::PoolQuery& pool_query,
                                               const DbOperation& db_op) {
    auto* ipc_manager = CHI_IPC;

    auto task = ipc_manager->NewTask<Mdm_insertTask>(
        chi::CreateTaskId(), pool_id_, pool_query, db_op);

    return ipc_manager->Send(task);
  }

  /**
   * Synchronous Mdm_insert wrapper
   * @param pool_query Pool routing information
   * @param db_op Database operation to execute
   */
  void Mdm_insert(const chi::PoolQuery& pool_query,
                  const DbOperation& db_op) {
    auto future = AsyncMdm_insert(pool_query, db_op);
    future.Wait();
    if (future->GetReturnCode() != 0) {
      HLOG(kError, "coeus_mdm::Mdm_insert failed with return code: {}",
            future->GetReturnCode());
    }
  }
};

}  // namespace chimaera::coeus_mdm

#endif  // COEUS_MDM_CLIENT_H_

