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

#include "comms/CTEHermes.h"
#include <cstdlib>
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
  
  // Initialize Chimaera runtime first
  if (!chi::CHIMAERA_INIT(chi::ChimaeraMode::kClient, true)) {
    std::cerr << "ERROR: Failed to initialize Chimaera runtime" << std::endl;
    return false;
  }
  
  // Get CTE config path from environment or use default
  std::string cte_config = getenv("CTE_CONFIG") ? getenv("CTE_CONFIG") : "";
  if (cte_config.empty()) {
    cte_config = "config/cte_config.yaml"; // Default config path
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
  
  // Create CTE container
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
  cte_client_->Init(create_task->new_pool_id_);
  
  // Register storage target (100MB file-based)
  auto reg_task = cte_client_->AsyncRegisterTarget(
      "/tmp/cte_storage",
      chimaera::bdev::BdevType::kFile,
      100 * 1024 * 1024);
  reg_task.Wait();
  if (reg_task->GetReturnCode() != 0) {
    // Warning only - target may already be registered or configured via config file
    std::cerr << "WARNING: Failed to register storage target (code: " 
              << reg_task->GetReturnCode() << ")" << std::endl;
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
  // CTE handles data placement automatically based on access patterns
  // Demote operation is handled by CTE's data placement engine
  // This is a no-op - CTE will automatically demote based on scoring
  (void)tag_name;  // Suppress unused parameter warning
  (void)blob_name; // Suppress unused parameter warning
  return true;
}

bool CTEHermes::Prefetch(const std::string &tag_name, const std::string &blob_name) {
  // CTE handles data placement automatically based on access patterns
  // Prefetch operation is handled by CTE's data placement engine
  // This is a no-op - CTE will automatically promote based on scoring
  (void)tag_name;  // Suppress unused parameter warning
  (void)blob_name; // Suppress unused parameter warning
  return true;
}

} // namespace coeus

