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

#ifndef COEUS_INCLUDE_COMMS_CTETAGCLIENT_H_
#define COEUS_INCLUDE_COMMS_CTETAGCLIENT_H_

#include "interfaces/ITag.h"
#include <wrp_cte/core/core_client.h>
#include <wrp_cte/core/core_tasks.h>
#include <string>
#include <vector>

namespace coeus {

/**
 * CTETagClient: ITag implementation using wrp_cte::core::Client directly
 * 
 * Unlike CTETag which wraps wrp_cte::core::Tag, this class uses
 * the CTE client API directly for tag/blob operations.
 */
class CTETagClient : public ITag {
 public:
  /**
   * Constructor - get or create tag by name
   * @param cte_client Pointer to CTE client (uses WRP_CTE_CLIENT if nullptr)
   * @param tag_name Name of the CTE tag
   */
  CTETagClient(wrp_cte::core::Client* cte_client, const std::string& tag_name);

  /**
   * Constructor - use existing tag by ID (e.g. from GetOrCreateTag())
   * @param cte_client Pointer to CTE client
   * @param tag_id Existing tag ID
   * @param tag_name Name of the tag (for ITag::name)
   */
  CTETagClient(wrp_cte::core::Client* cte_client,
               const wrp_cte::core::TagId& tag_id,
               const std::string& tag_name);

  /**
   * Destructor
   */
  ~CTETagClient() override = default;

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
  wrp_cte::core::Client* cte_client_;  // CTE client (borrowed, not owned)
  wrp_cte::core::TagId tag_id_;        // Tag ID
  std::string tag_name_;               // Tag name
  
  /**
   * Get default blob score for data placement
   */
  float GetDefaultBlobScore() const { return 0.7f; }
};

} // namespace coeus

#endif // COEUS_INCLUDE_COMMS_CTETAGCLIENT_H_

