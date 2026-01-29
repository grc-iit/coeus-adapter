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
#include <cstdlib>
#include <cstring>
#include <iostream>

namespace coeus {

CTEHermes::CTEHermes(wrp_cte::core::Client* cte_client)
    : cte_client_(cte_client), is_connected_(false) {
  // Use provided client or global client
  if (!cte_client_) {
    cte_client_ = WRP_CTE_CLIENT;
  }
  
  // Initialize tag pointer to nullptr
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
  if (is_connected_ && cte_client_) {
    return true; // Already connected
  }
  
  // Check for CHIMAERA_WITH_RUNTIME environment variable and warn if set
  const char* with_runtime_env = std::getenv("CHIMAERA_WITH_RUNTIME");
  if (with_runtime_env && (std::strcmp(with_runtime_env, "1") == 0 || 
                           std::strcmp(with_runtime_env, "true") == 0 ||
                           std::strcmp(with_runtime_env, "TRUE") == 0)) {
    std::cout << "WARNING: CHIMAERA_WITH_RUNTIME=1 is set. This will start runtime on every MPI rank." << std::endl;
    std::cout << "  This may cause port conflicts. Consider unsetting it or starting runtime separately." << std::endl;
  }
  
  // Initialize Chimaera client (not runtime)
  // Note: default_with_runtime=false is correct for production MPI runs
  // WRP_CTE_CLIENT_INIT will handle Chimaera initialization internally
  // Only set to true for unit tests or if explicitly needed
  if (!chi::CHIMAERA_INIT(chi::ChimaeraMode::kClient, false)) {
    std::cerr << "ERROR: Failed to initialize Chimaera client" << std::endl;
    std::cerr << "  Make sure Chimaera runtime is running (start with: chimaera_start_runtime)" << std::endl;
    return false;
  }
  
  // Get CTE config path from environment or use default
  const char* cte_config_env = std::getenv("CTE_CONFIG");
  std::string cte_config = cte_config_env ? cte_config_env : "";
  if (cte_config.empty()) {
    cte_config = "config/cte_config.yaml";
  }
  
  // Initialize CTE subsystem
  if (!wrp_cte::core::WRP_CTE_CLIENT_INIT(cte_config, chi::PoolQuery::Dynamic())) {
    std::cerr << "ERROR: Failed to initialize CTE subsystem" << std::endl;
    return false;
  }
  
  // Get CTE client
  if (!cte_client_) {
    cte_client_ = WRP_CTE_CLIENT;
  }
  
  if (!cte_client_) {
    std::cerr << "ERROR: CTE client is null after initialization" << std::endl;
    return false;
  }

  // Pre-deployed CTE: when runtime and CTE core are already started (e.g. Jarvis
  // with cte_core pool_id: 512.0), attach to the existing pool instead of Create.
  const char* pre_deployed = std::getenv("CTE_PRE_DEPLOYED");
  const bool use_pre_deployed = pre_deployed && (std::strcmp(pre_deployed, "1") == 0 ||
                                                 std::strcmp(pre_deployed, "true") == 0 ||
                                                 std::strcmp(pre_deployed, "TRUE") == 0);

  if (use_pre_deployed) {
    // Use existing CTE core pool (must match pre-deployed config, e.g. pool_id: 512.0)
    cte_client_->pool_id_ = wrp_cte::core::kCtePoolId;
    cte_client_->Init(wrp_cte::core::kCtePoolId);
  } else {
    // Create CTE container (or GetOrCreate if already exists)
    wrp_cte::core::CreateParams params;
    auto create_task = cte_client_->AsyncCreate(
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
    cte_client_->pool_id_ = create_task->new_pool_id_;
    cte_client_->Init(create_task->new_pool_id_);

    // Register storage target (100MB file-based)
    chi::PoolId bdev_id(514, 0);
    auto reg_task = cte_client_->AsyncRegisterTarget(
        "/tmp/cte_storage",
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
  if (!cte_client_) {
    std::cerr << "ERROR: CTE client not initialized. Call connect() first." << std::endl;
    return false;
  }
  
  // Clean up previous tag if exists
  if (tag) {
    delete tag;
    tag = nullptr;
  }
  
  // Create new CTETagClient using CTE client directly
  try {
    tag = new CTETagClient(cte_client_, tag_name);
    return true;
  } catch (const std::exception& e) {
    std::cerr << "ERROR: Failed to create tag '" << tag_name << "': " << e.what() << std::endl;
    tag = nullptr;
    return false;
  }
}

bool CTEHermes::Demote(const std::string &tag_name, const std::string &blob_name) {
  if (!cte_client_) {
    std::cerr << "ERROR: CTE client not initialized. Call connect() first." << std::endl;
    return false;
  }
  
  try {
    // Get or create tag to obtain TagId
    auto tag_task = cte_client_->AsyncGetOrCreateTag(tag_name);
    tag_task.Wait();
    
    if (tag_task->GetReturnCode() != 0) {
      std::cerr << "ERROR: Failed to get tag '" << tag_name << "' for demote operation" << std::endl;
      return false;
    }
    
    wrp_cte::core::TagId tag_id = tag_task->tag_id_;
    
    // Demote: lower score (0.3 = cold tier) to move blob to slower storage
    float demote_score = 0.3f;
    auto reorganize_task = cte_client_->AsyncReorganizeBlob(tag_id, blob_name, demote_score);
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
  if (!cte_client_) {
    std::cerr << "ERROR: CTE client not initialized. Call connect() first." << std::endl;
    return false;
  }
  
  try {
    // Get or create tag to obtain TagId
    auto tag_task = cte_client_->AsyncGetOrCreateTag(tag_name);
    tag_task.Wait();
    
    if (tag_task->GetReturnCode() != 0) {
      std::cerr << "ERROR: Failed to get tag '" << tag_name << "' for prefetch operation" << std::endl;
      return false;
    }
    
    wrp_cte::core::TagId tag_id = tag_task->tag_id_;
    
    // Prefetch: higher score (0.95 = hot tier) to move blob to faster storage
    float prefetch_score = 0.95f;
    auto reorganize_task = cte_client_->AsyncReorganizeBlob(tag_id, blob_name, prefetch_score);
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

