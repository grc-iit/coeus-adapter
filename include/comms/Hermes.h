/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Distributed under BSD 3-Clause license.                                   *
 * Copyright by the Illinois Institute of Technology.                        *
 * All rights reserved.                                                      *
 *                                                                           *
 * This file is part of Coeus-adapter. The full Coeus-adapter copyright      *
 * notice, including terms governing use, modification, and redistribution,  *
 * is contained in the COPYING file, which can be found at the top directory.*
 * If you do not have access to the file, you may request a copy             *
 * from scslab@iit.edu.                                                      *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef COEUS_INCLUDE_COMMS_HERMES_H_
#define COEUS_INCLUDE_COMMS_HERMES_H_

#include "interfaces/IHermes.h"
#include "CTETagClient.h"
#include "common/Tracer.h"
#include <wrp_cte/core/core_client.h>
#include <wrp_cte/core/core_tasks.h>
#include <chimaera/chimaera.h>
#include <cstdlib>

namespace coeus {
class Hermes : public IHermes {
 public:
  float promote_weight = 0.25;
  float demote_weight = -0.25;
  bool use_cte_ = true;  // Using CTE for I/O operations

  Hermes() = default;

  bool connect() override {
    std::cout << "Entering connect" << std::endl;
    
    // Initialize Context-Transfer-Engine for I/O operations
    std::cout << "Initializing Context-Transfer-Engine (CTE) for I/O" << std::endl;
    
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
    
    // Get CTE client and create container
    auto* cte_client = WRP_CTE_CLIENT;
    if (!cte_client) {
      std::cerr << "ERROR: CTE client is null after initialization" << std::endl;
      return false;
    }
    
    // Create CTE container
    wrp_cte::core::CreateParams params;
    auto create_task = cte_client->AsyncCreate(
        chi::PoolQuery::Dynamic(),
        wrp_cte::core::kCtePoolName,
        wrp_cte::core::kCtePoolId,
        params);
    create_task.Wait();
    if (create_task->GetReturnCode() != 0) {
      std::cerr << "ERROR: Failed to create CTE container" << std::endl;
      return false;
    }
    cte_client->Init(create_task->new_pool_id_);
    
    // Register storage target (100MB file-based)
    auto reg_task = cte_client->AsyncRegisterTarget(
        "/tmp/cte_storage",
        chimaera::bdev::BdevType::kFile,
        100 * 1024 * 1024);
    reg_task.Wait();
    if (reg_task->GetReturnCode() != 0) {
      // Warning only - target may already be registered or configured via config file
      std::cerr << "WARNING: Failed to register storage target (code: " 
                << reg_task->GetReturnCode() << ")" << std::endl;
    }
    
    std::cout << "CTE initialized successfully with config: " << cte_config << std::endl;
    use_cte_ = true;
    return true;
  };

    bool GetTag(const std::string &tag_name) override {
    // Create CTE tag for blob storage using CTETagClient
    auto* cte_client = WRP_CTE_CLIENT;
    if (!cte_client) {
      std::cerr << "ERROR: CTE client is null in GetTag" << std::endl;
      return false;
    }
    tag = new coeus::CTETagClient(cte_client, tag_name);
    return true;
  }

  bool Demote(const std::string &tag_name, const std::string &blob_name) override {
    // CTE handles data placement automatically based on access patterns
    // Demote operation is handled by CTE's data placement engine
    // This is a no-op for now - CTE will automatically demote based on scoring
    std::cout << "Demote called for " << tag_name << "/" << blob_name 
              << " - CTE handles placement automatically" << std::endl;
    return true;
  }

  bool Prefetch(const std::string &tag_name, const std::string &blob_name) override {
    // CTE handles data placement automatically based on access patterns
    // Prefetch operation is handled by CTE's data placement engine
    // This is a no-op for now - CTE will automatically promote based on scoring
    std::cout << "Prefetch called for " << tag_name << "/" << blob_name 
              << " - CTE handles placement automatically" << std::endl;
    return true;
  }
};

}
#endif //COEUS_INCLUDE_COMMS_HERMES_H_
