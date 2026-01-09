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
#include "CTEBucket.h"
#include "common/Tracer.h"
#include <wrp_cte/core/core_client.h>
#include <cstdlib>

namespace coeus {
class Hermes : public IHermes {
 public:
  float promote_weight = 0.25;
  float demote_weight = -0.25;
  bool use_cte_ = true;  // Flag to use CTE instead of Hermes I/O

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
    
    // WRP_CTE_CLIENT_INIT automatically calls chi::CHIMAERA_INIT internally
    bool cte_init = wrp_cte::core::WRP_CTE_CLIENT_INIT(cte_config);
    if (!cte_init) {
      std::cerr << "ERROR: Failed to initialize CTE. CTE is required for I/O operations." << std::endl;
      return false;
    }
    
    std::cout << "CTE initialized successfully with config: " << cte_config << std::endl;
    use_cte_ = true;
    return true;
  };

    bool GetBucket(const std::string &bucket_name) override {
    // Always use CTE-based bucket (Hermes I/O has been migrated to CTE)
    bkt = (IBucket*) new coeus::CTEBucket(bucket_name);
    return true;
  }

  bool Demote(const std::string &bucket_name, const std::string &blob_name) override {
    // CTE handles data placement automatically based on access patterns
    // Demote operation is handled by CTE's data placement engine
    // This is a no-op for now - CTE will automatically demote based on scoring
    std::cout << "Demote called for " << bucket_name << "/" << blob_name 
              << " - CTE handles placement automatically" << std::endl;
    return true;
  }

  bool Prefetch(const std::string &bucket_name, const std::string &blob_name) override {
    // CTE handles data placement automatically based on access patterns
    // Prefetch operation is handled by CTE's data placement engine
    // This is a no-op for now - CTE will automatically promote based on scoring
    std::cout << "Prefetch called for " << bucket_name << "/" << blob_name 
              << " - CTE handles placement automatically" << std::endl;
    return true;
  }
};

}
#endif //COEUS_INCLUDE_COMMS_HERMES_H_
