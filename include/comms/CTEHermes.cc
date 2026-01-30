/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Distributed under BSD 3-Clause license.                                   *
 * Copyright by the Illinois Institute of Technology.                        *
 * All rights reserved.                                                      *
 *                                                                           *
 * This file is part of Coeus-adapter. The full Coeus-adapter copyright      *
 * notice, including terms governing use, modification, and redistribution,  *
 * is contained in the COPYING file, which can be found at the top directory.*
 * If you do not have access to the file, you may request a copy             *
 * from scslab@iit.edu.                                                      *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/**
 * CTEHermes: adapter between Coeus Hermes engine and the CTE (Context Transfer Engine).
 *
 * connect() supports two modes:
 * - CTE_PRE_DEPLOYED=1: Attach to an existing CTE core pool (e.g. pool_id 512.0
 *   started by Jarvis). Skips Create and RegisterTarget; uses kCtePoolId (512,0).
 * - Otherwise: Create (or GetOrCreate) CTE container and register /tmp/cte_storage.
 *   pool_id_ is always set from the task so PutBlob/GetBlob use the correct pool.
 */

#include "comms/CTEHermes.h"
#include "comms/CTETagClient.h"
#include <cstdlib>
#include <cstring>
#include <iostream>

namespace coeus {

CTEHermes::CTEHermes() : is_connected_(false) {
  // Initialize tag pointer to nullptr (inherited from IHermes)
  tag = nullptr;
}

CTEHermes::~CTEHermes() {
  // Clean up tag if it exists
  if (tag) {
    delete tag;
    tag = nullptr;
  }
}

bool CTEHermes::connect() {
  if (is_connected_) {
    return true;  // Already connected
  }

  // Initialize CTE subsystem (Chimaera + global client if needed)
  if (!wrp_cte::core::WRP_CTE_CLIENT_INIT("", chi::PoolQuery::Local())) {
    std::cerr << "ERROR: Failed to initialize CTE subsystem" << std::endl;
    return false;
  }

  // Pre-deployed CTE: when runtime and CTE core are already started (e.g. Jarvis
  // with cte_core pool_id: 512.0), attach this client to the existing pool.
  const char* pre_deployed = std::getenv("CTE_PRE_DEPLOYED");
  const bool use_pre_deployed = pre_deployed && (std::strcmp(pre_deployed, "1") == 0 ||
                                                 std::strcmp(pre_deployed, "true") == 0 ||
                                                 std::strcmp(pre_deployed, "TRUE") == 0);

  if (use_pre_deployed) {
    // Use existing CTE core pool (must match pre-deployed config, e.g. pool_id: 512.0)
    pool_id_ = wrp_cte::core::kCtePoolId;
    Init(wrp_cte::core::kCtePoolId);
  } else {
    // Create CTE container (or GetOrCreate if already exists)
    wrp_cte::core::CreateParams params;
    auto create_task = AsyncCreate(
        chi::PoolQuery::Dynamic(),
        wrp_cte::core::kCtePoolName,
        wrp_cte::core::kCtePoolId,
        params);
    create_task.Wait();
    if (create_task->GetReturnCode() != 0) {
      std::cerr << "ERROR: Failed to create CTE container" << std::endl;
      return false;
    }
    // CRITICAL: Set pool_id_ so PutBlob/GetBlob tasks use the correct pool.
    pool_id_ = create_task->new_pool_id_;
    Init(create_task->new_pool_id_);

    // Register storage target (100MB file-based)
    chi::PoolId bdev_id(514, 0);
    auto reg_task = AsyncRegisterTarget(
        "/mnt/common/hxu40/cte_storage",
        chimaera::bdev::BdevType::kFile,
        100 * 1024 * 1024,
        chi::PoolQuery::Local(),
        bdev_id);
    reg_task.Wait();
    if (reg_task->GetReturnCode() != 0) {
      std::cout << "WARNING: Failed to register storage target (code: "
                << reg_task->GetReturnCode() << ")" << std::endl;
    }
  }

  is_connected_ = true;
  return true;
}

bool CTEHermes::GetTag(const std::string &tag_name) {
  if (!is_connected_) {
    std::cerr << "ERROR: CTE not connected. Call connect() first." << std::endl;
    return false;
  }

  // Clean up previous tag if exists
  if (tag) {
    delete tag;
    tag = nullptr;
  }

  // Get or create tag using AsyncGetOrCreateTag + Wait (Client in your build has no GetOrCreateTag)
  try {
    auto tag_task = AsyncGetOrCreateTag(tag_name);
    tag_task.Wait();
    if (tag_task->GetReturnCode() != 0) {
      std::cerr << "ERROR: GetOrCreateTag failed for '" << tag_name << "'" << std::endl;
      return false;
    }
    wrp_cte::core::TagId tag_id = tag_task->tag_id_;
    current_tag_id_ = tag_id;
    tag = new CTETagClient(this, tag_id, tag_name);
    return true;
  } catch (const std::exception& e) {
    std::cerr << "ERROR: Failed to create tag '" << tag_name << "': " << e.what() << std::endl;
    tag = nullptr;
    return false;
  }
}

bool CTEHermes::Put(const std::string &blob_name, size_t blob_size, const void *values) {
  if (!is_connected_) {
    std::cerr << "ERROR: CTE not connected. Call connect() first." << std::endl;
    return false;
  }
  try {
    auto *ipc_manager = CHI_IPC;
    hipc::FullPtr<char> shm_fullptr = ipc_manager->AllocateBuffer(blob_size);
    if (shm_fullptr.IsNull()) {
      std::cerr << "ERROR: Failed to allocate shared memory for PutBlob" << std::endl;
      return false;
    }
    std::memcpy(shm_fullptr.ptr_, values, blob_size);
    hipc::ShmPtr<> shm_ptr(shm_fullptr.shm_);
    float score = 0.7f;
    auto task = AsyncPutBlob(current_tag_id_, blob_name, 0, blob_size, shm_ptr, score, 0);
    task.Wait();
    ipc_manager->FreeBuffer(shm_fullptr);
    if (task->GetReturnCode() != 0) {
      std::cerr << "ERROR: PutBlob failed for blob '" << blob_name << "'" << std::endl;
      return false;
    }
    return true;
  } catch (const std::exception &e) {
    std::cerr << "ERROR: CTEHermes::Put failed for '" << blob_name << "': " << e.what() << std::endl;
    return false;
  }
}

bool CTEHermes::Demote(const std::string &tag_name, const std::string &blob_name) {
  if (!is_connected_) {
    std::cerr << "ERROR: CTE not connected. Call connect() first." << std::endl;
    return false;
  }

  try {
    auto tag_task = AsyncGetOrCreateTag(tag_name);
    tag_task.Wait();
    if (tag_task->GetReturnCode() != 0) {
      std::cerr << "ERROR: Failed to get tag '" << tag_name << "' for demote operation" << std::endl;
      return false;
    }
    wrp_cte::core::TagId tag_id = tag_task->tag_id_;

    // Demote: lower score (0.3 = cold tier) to move blob to slower storage
    float demote_score = 0.3f;
    auto reorganize_task = AsyncReorganizeBlob(tag_id, blob_name, demote_score);
    reorganize_task.Wait();
    
    if (reorganize_task->GetReturnCode() == 0) {
      return true;
    } else {
      std::cerr << "WARNING: ReorganizeBlob failed for blob '" << blob_name 
                << "' in tag '" << tag_name << "' (code: " 
                << reorganize_task->GetReturnCode() << ")" << std::endl;
      return false;
    }
  } catch (const std::exception& e) {
    std::cerr << "ERROR: Demote failed for blob '" << blob_name 
              << "' in tag '" << tag_name << "': " << e.what() << std::endl;
    return false;
  }
}

bool CTEHermes::Prefetch(const std::string &tag_name, const std::string &blob_name) {
  if (!is_connected_) {
    std::cerr << "ERROR: CTE not connected. Call connect() first." << std::endl;
    return false;
  }

  try {
    auto tag_task = AsyncGetOrCreateTag(tag_name);
    tag_task.Wait();
    if (tag_task->GetReturnCode() != 0) {
      std::cerr << "ERROR: Failed to get tag '" << tag_name << "' for prefetch operation" << std::endl;
      return false;
    }
    wrp_cte::core::TagId tag_id = tag_task->tag_id_;

    // Prefetch: higher score (0.95 = hot tier) to move blob to faster storage
    float prefetch_score = 0.95f;
    auto reorganize_task = AsyncReorganizeBlob(tag_id, blob_name, prefetch_score);
    reorganize_task.Wait();
    
    if (reorganize_task->GetReturnCode() == 0) {
      return true;
    } else {
      std::cerr << "WARNING: ReorganizeBlob failed for blob '" << blob_name 
                << "' in tag '" << tag_name << "' (code: " 
                << reorganize_task->GetReturnCode() << ")" << std::endl;
      return false;
    }
  } catch (const std::exception& e) {
    std::cerr << "ERROR: Prefetch failed for blob '" << blob_name 
              << "' in tag '" << tag_name << "': " << e.what() << std::endl;
    return false;
  }
}

} // namespace coeus

