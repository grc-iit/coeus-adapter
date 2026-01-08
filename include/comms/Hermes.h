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
#include "Bucket.h"
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
    
    if (use_cte_) {
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
        std::cerr << "Failed to initialize CTE, falling back to Hermes" << std::endl;
        use_cte_ = false;
      } else {
        std::cout << "CTE initialized successfully with config: " << cte_config << std::endl;
        return true;
      }
    }
    
    // Fallback to Hermes if CTE initialization failed or disabled
    if (!use_cte_) {
      std::cout << "HERMES_CONF: " << getenv("HERMES_CONF") << std::endl;
      
      // Initialize Hermes storage backend (not task management)
      TRANSPARENT_HERMES();
      hermes = HERMES;
      
      // Note: Task management is now handled by Chimaera (Context-Runtime)
      // Module registration is done via chimaera_mod.yaml files, not here
      
      std::cout << "Connected to Hermes storage backend" << std::endl;
      return hermes->IsInitialized();
    }
    
    return true;
  };

    bool GetBucket(const std::string &bucket_name) override {
    if (use_cte_) {
      // Use CTE-based bucket
      bkt = (IBucket*) new coeus::CTEBucket(bucket_name);
    } else {
      // Use Hermes-based bucket (fallback)
      bkt = (IBucket*) new coeus::Bucket(bucket_name, this);
    }
    return true;
  }

  bool Demote(const std::string &bucket_name, const std::string &blob_name) override {


    hapi::Context ctx;
    auto bkt = hermes->GetBucket(bucket_name);

    hermes::BlobId blob_id = bkt.GetBlobId(blob_name);
    float blob_score = bkt.GetBlobScore(blob_id);

    bkt.ReorganizeBlob(blob_id, blob_score + demote_weight, blob_score, ctx);
  }

  bool Prefetch(const std::string &bucket_name, const std::string &blob_name) override {

    hapi::Context ctx;
    auto bkt = hermes->GetBucket(bucket_name);

    hermes::BlobId blob_id = bkt.GetBlobId(blob_name);
    float blob_score = bkt.GetBlobScore(blob_id);

    bkt.ReorganizeBlob(blob_id, 1, blob_score + promote_weight, ctx);
  }
};

}
#endif //COEUS_INCLUDE_COMMS_HERMES_H_
