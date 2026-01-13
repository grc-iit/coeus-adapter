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

#ifndef COEUS_INCLUDE_COMMS_CTETAG_H_
#define COEUS_INCLUDE_COMMS_CTETAG_H_

#include "interfaces/ITag.h"
#include <wrp_cte/core/core_client.h>
#include <wrp_cte/core/core_tasks.h>
#include <string>
#include <vector>

namespace coeus {

/**
 * CTETag: CTE Tag implementation for blob storage
 * 
 * This class implements the ITag interface using CTE (Context-Transfer-Engine)
 * for intelligent data placement across storage tiers. It wraps CTE Tag
 * and provides CTE blob storage/retrieval operations.
 */
class CTETag : public ITag {
 public:
  /**
   * Constructor
   * @param tag_name Name of the CTE tag
   */
  explicit CTETag(const std::string &tag_name);

  /**
   * Destructor
   */
  ~CTETag() override = default;

  /**
   * Put CTE blob data into the tag
   * @param blob_name Name of the blob
   * @param blob_size Size of the data in bytes
   * @param values Pointer to the data
   */
  void Put(const std::string &blob_name, size_t blob_size, const void* values) override;

  /**
   * Get CTE blob data from the tag by name
   * @param blob_name Name of the blob
   * @return Vector containing the blob data (empty if blob not found)
   */
  std::vector<uint8_t> Get(const std::string &blob_name) override;

  /**
   * Get all blob names contained in this tag
   * @return Vector of blob names
   */
  std::vector<std::string> GetContainedBlobNames() override;

  /**
   * Get blob size
   * @param blob_name Name of the blob
   * @return Size of the blob in bytes, or 0 if blob not found
   */
  size_t GetBlobSize(const std::string &blob_name) override;

 private:
  wrp_cte::core::Tag tag_;  // CTE tag
  std::string tag_name_;    // Tag name
  
  /**
   * Get default blob score for data placement
   * Can be customized based on access patterns, size, etc.
   */
  float GetDefaultBlobScore() const { return 0.7f; } // Default: warm data
};

} // namespace coeus

#endif // COEUS_INCLUDE_COMMS_CTETAG_H_

