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
#include "CTETag.h"
#include "CTEInitializer.h"
#include "common/Tracer.h"
#include <wrp_cte/core/core_client.h>
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
    
    // Get CTE config path from environment or use default
    std::string cte_config = getenv("CTE_CONFIG") ? getenv("CTE_CONFIG") : "";
    if (cte_config.empty()) {
      cte_config = "config/cte_config.yaml"; // Default config path
    }
    
    // Use CTEInitializer for proper initialization following CTE best practices
    // This ensures:
    // 1. Chimaera is initialized first
    // 2. CTE subsystem is initialized
    // 3. CTE client pool is created
    bool cte_init = CTEInitializer::Initialize(cte_config, chi::PoolQuery::Dynamic());
    if (!cte_init) {
      std::cerr << "ERROR: Failed to initialize CTE. CTE is required for I/O operations." << std::endl;
      return false;
    }
    
    // Optionally register storage targets if specified in environment
    // This can be done here or left to configuration file
    const char* storage_path = getenv("CTE_STORAGE_PATH");
    if (storage_path) {
      chi::u64 storage_size = 100ULL * 1024 * 1024 * 1024; // Default 100GB
      const char* storage_size_str = getenv("CTE_STORAGE_SIZE");
      if (storage_size_str) {
        storage_size = std::stoull(storage_size_str);
      }
      
      if (!CTEInitializer::RegisterStorageTarget(
          storage_path, 
          chimaera::bdev::BdevType::kFile, 
          storage_size)) {
        std::cerr << "WARNING: Failed to register storage target: " << storage_path << std::endl;
        // Continue anyway - CTE may have targets configured via config file
      }
    }
    
    std::cout << "CTE initialized successfully with config: " << cte_config << std::endl;
    use_cte_ = true;
    return true;
  };

    bool GetTag(const std::string &tag_name) override {
    // Create CTE tag for blob storage
    tag = (ITag*) new coeus::CTETag(tag_name);
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
