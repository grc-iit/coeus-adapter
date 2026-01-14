/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Distributed under BSD 3-Clause license.                                   *
 * Copyright by the Illinois Institute of Technology.                        *
 * All rights reserved.                                                      *
 *                                                                           *
 * This file is part of Coeus-adapter. The full Coeus-adapter copyright      *
 * notice, including terms governing use, modification, and redistribution,  *
 * is contained in the COPYING file, which can be found at the top directory.*
 * If you do not have access to the file, you may request a copy             *
 * from scslab@iit.edu.                                                      *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef COEUS_INCLUDE_COMMS_CTEINITIALIZER_H_
#define COEUS_INCLUDE_COMMS_CTEINITIALIZER_H_

#include <wrp_cte/core/core_client.h>
#include <chimaera/chimaera.h>
#include <chimaera/chimaera_manager.h>
#include <string>
#include <iostream>

namespace coeus {

/**
 * CTEInitializer - Helper class for proper CTE initialization
 * 
 * Follows the pattern from CTE documentation:
 * 1. Initialize Chimaera runtime
 * 2. Initialize CTE subsystem
 * 3. Create CTE client pool
 * 4. Register storage targets (optional)
 */
class CTEInitializer {
 public:
  /**
   * Initialize CTE with proper setup
   * @param config_path Optional path to CTE config file (can be empty)
   * @param pool_query Pool query for CTE container creation (default: Dynamic)
   * @return true if initialization successful, false otherwise
   */
  static bool Initialize(const std::string &config_path = "",
                         const chi::PoolQuery &pool_query = chi::PoolQuery::Dynamic()) {
    // Step 1: Initialize Chimaera runtime (if not already initialized)
    // Note: WRP_CTE_CLIENT_INIT will call this internally, but we can call it explicitly
    // for clarity and to ensure it's done first
    if (!CHI_CHIMAERA_MANAGER || !CHI_CHIMAERA_MANAGER->IsInitialized()) {
      bool chimaera_success = chi::CHIMAERA_INIT(chi::ChimaeraMode::kClient, true);
      if (!chimaera_success) {
        std::cerr << "ERROR: Failed to initialize Chimaera runtime" << std::endl;
        return false;
      }
    }

    // Step 2: Initialize CTE subsystem
    // WRP_CTE_CLIENT_INIT internally:
    //   - Creates the global CTE client instance
    //   - Calls ContentTransferEngine::ClientInit which creates the CTE pool
    bool cte_init = wrp_cte::core::WRP_CTE_CLIENT_INIT(config_path, pool_query);
    if (!cte_init) {
      std::cerr << "ERROR: Failed to initialize CTE subsystem" << std::endl;
      return false;
    }

    // Step 3: Verify CTE client is available
    auto *cte_client = WRP_CTE_CLIENT;
    if (!cte_client) {
      std::cerr << "ERROR: CTE client is null after initialization" << std::endl;
      return false;
    }

    return true;
  }

  /**
   * Register a storage target for CTE
   * @param target_path Path to the storage target (file path for file-based storage)
   * @param bdev_type Block device type (kFile, kRam, etc.)
   * @param target_size Size of the target in bytes
   * @return true if registration successful, false otherwise
   */
  static bool RegisterStorageTarget(const std::string &target_path,
                                    chimaera::bdev::BdevType bdev_type,
                                    chi::u64 target_size) {
    auto *cte_client = WRP_CTE_CLIENT;
    if (!cte_client) {
      std::cerr << "ERROR: CTE client not initialized. Call Initialize() first." << std::endl;
      return false;
    }

    hipc::MemContext mctx;
    chi::u32 result = cte_client->RegisterTarget(mctx, target_path, bdev_type, target_size);
    
    if (result != 0) {
      std::cerr << "ERROR: Failed to register storage target: " << target_path 
                << " (error code: " << result << ")" << std::endl;
      return false;
    }

    return true;
  }

  /**
   * Check if CTE is initialized
   * @return true if CTE client is available, false otherwise
   */
  static bool IsInitialized() {
    return (WRP_CTE_CLIENT != nullptr);
  }
};

} // namespace coeus

#endif // COEUS_INCLUDE_COMMS_CTEINITIALIZER_H_

