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

#ifndef COEUS_INCLUDE_COMMS_CTEBUCKET_H_
#define COEUS_INCLUDE_COMMS_CTEBUCKET_H_

#include "interfaces/IBucket.h"
#include <wrp_cte/core/core_client.h>
#include <wrp_cte/core/core_tasks.h>
#include <hermes/hermes_types.h>
#include <string>
#include <vector>
#include <map>
#include <mutex>

namespace coeus {

/**
 * CTEBucket: Context-Transfer-Engine based bucket implementation
 * 
 * This class implements the IBucket interface using CTE (Context-Transfer-Engine)
 * for intelligent data placement across storage tiers. It maps bucket names to
 * CTE tags and provides blob storage/retrieval operations.
 */
class CTEBucket : public IBucket {
 public:
  /**
   * Constructor
   * @param bucket_name Name of the bucket (maps to CTE tag name)
   */
  explicit CTEBucket(const std::string &bucket_name);

  /**
   * Destructor
   */
  ~CTEBucket() override = default;

  /**
   * Put blob data into the bucket
   * @param blob_name Name of the blob
   * @param blob_size Size of the data in bytes
   * @param values Pointer to the data
   */
  void Put(const std::string &blob_name, size_t blob_size, const void* values) override;

  /**
   * Get blob data from the bucket by name
   * @param blob_name Name of the blob
   * @return hermes::Blob containing the data
   */
  hermes::Blob Get(const std::string &blob_name) override;

  /**
   * Get blob data from the bucket by blob ID
   * Note: CTE doesn't use blob IDs in the same way as Hermes.
   * This method maintains interface compatibility but requires blob name lookup.
   * @param blob_id Hermes blob ID (converted to name via internal mapping)
   * @return hermes::Blob containing the data
   */
  hermes::Blob Get(hermes::BlobId blob_id) override;

  /**
   * Get all blob IDs contained in this bucket
   * @return Vector of hermes::BlobId (converted from CTE blob names)
   */
  std::vector<hermes::BlobId> GetContainedBlobIds() override;

  /**
   * Get blob ID from blob name
   * @param blob_name Name of the blob
   * @return hermes::BlobId (generated from blob name for compatibility)
   */
  hermes::BlobId GetBlobId(const std::string &blob_name) override;

  /**
   * Get blob name from blob ID
   * @param blob_id Hermes blob ID
   * @return Blob name (looked up from internal mapping)
   */
  std::string GetBlobName(const hermes::BlobId &blob_id) override;

 private:
  wrp_cte::core::Tag tag_;  // CTE tag (equivalent to bucket)
  std::string bucket_name_; // Original bucket name
  
  // Mapping between Hermes BlobId and blob names for compatibility
  std::map<hermes::BlobId, std::string> blob_id_to_name_;
  std::map<std::string, hermes::BlobId> blob_name_to_id_;
  std::mutex mapping_mutex_; // Protect mapping updates
  
  /**
   * Generate a Hermes::BlobId from blob name (for compatibility)
   * Uses hash of blob name to create a deterministic ID
   */
  hermes::BlobId GenerateBlobId(const std::string &blob_name);
  
  /**
   * Update internal blob ID mappings
   */
  void UpdateBlobMapping(const std::string &blob_name);
  
  /**
   * Get default blob score for data placement
   * Can be customized based on access patterns, size, etc.
   */
  float GetDefaultBlobScore() const { return 0.7f; } // Default: warm data
};

} // namespace coeus

#endif // COEUS_INCLUDE_COMMS_CTEBUCKET_H_

