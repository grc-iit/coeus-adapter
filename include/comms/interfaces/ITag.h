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

#ifndef COEUS_INCLUDE_COMMS_INTERFACES_ITAG_H_
#define COEUS_INCLUDE_COMMS_INTERFACES_ITAG_H_

#include <vector>
#include <string>
#include <cstdint>

namespace coeus {
/**
 * ITag - Interface for CTE blob storage operations using CTE Tags
 * 
 * This interface is CTE-native. CTE Tags are containers for blobs.
 * Blobs are identified by name (string), not by ID.
 */
class ITag {
 public:
  std::string name;
  virtual ~ITag() = default;

  /**
   * Put CTE blob data into the tag
   * @param blob_name Name of the blob
   * @param blob_size Size of the data in bytes
   * @param values Pointer to the data
   */
  virtual void Put(const std::string &blob_name, size_t blob_size, const void *values) = 0;

  /**
   * Get CTE blob data from the tag by name
   * @param blob_name Name of the blob
   * @return Vector containing the blob data (empty if blob not found)
   */
  virtual std::vector<uint8_t> Get(const std::string &blob_name) = 0;

  /**
   * Get all blob names contained in this tag
   * @return Vector of blob names
   */
  virtual std::vector<std::string> GetContainedBlobNames() = 0;

  /**
   * Get blob size
   * @param blob_name Name of the blob
   * @return Size of the blob in bytes, or 0 if blob not found
   */
  virtual size_t GetBlobSize(const std::string &blob_name) = 0;
};
}
#endif //COEUS_INCLUDE_COMMS_INTERFACES_ITAG_H_
