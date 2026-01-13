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

#include "comms/CTETag.h"
#include <cstring>
#include <stdexcept>
#include <functional>
#include <iostream>

namespace coeus {

CTETag::CTETag(const std::string &tag_name) 
    : tag_name_(tag_name), tag_(tag_name) {
  name = tag_name;
  
  // Ensure CTE client is initialized
  if (!WRP_CTE_CLIENT) {
    throw std::runtime_error("CTE client not initialized. Call WRP_CTE_CLIENT_INIT() first.");
  }
}

void CTETag::Put(const std::string &blob_name, size_t blob_size, const void* values) {
  try {
    // CTE Tag::PutBlob handles shared memory automatically for sync operations
    // Uses default score for data placement
    tag_.PutBlob(blob_name, static_cast<const char*>(values), blob_size, 0);
  } catch (const std::exception& e) {
    std::cerr << "CTE PutBlob failed for blob '" << blob_name 
              << "' in tag '" << tag_name_ << "': " << e.what() << std::endl;
    throw std::runtime_error("CTE PutBlob failed: " + std::string(e.what()));
  }
}

std::vector<uint8_t> CTETag::Get(const std::string &blob_name) {
  try {
    // Get blob size first
    chi::u64 blob_size = tag_.GetBlobSize(blob_name);
    if (blob_size == 0) {
      // Blob doesn't exist or is empty
      return std::vector<uint8_t>();
    }
    
    // Allocate buffer for data
    std::vector<uint8_t> buffer(blob_size);
    
    // Retrieve blob data directly into buffer
    tag_.GetBlob(blob_name, reinterpret_cast<char*>(buffer.data()), blob_size, 0);
    
    return buffer;
  } catch (const std::exception& e) {
    std::cerr << "CTE GetBlob failed for blob '" << blob_name 
              << "' in tag '" << tag_name_ << "': " << e.what() << std::endl;
    return std::vector<uint8_t>(); // Return empty vector on error
  }
}

std::vector<std::string> CTETag::GetContainedBlobNames() {
  try {
    // Get all blob names from CTE tag
    return tag_.GetContainedBlobs();
  } catch (const std::exception& e) {
    std::cerr << "CTE GetContainedBlobs failed for tag '" 
              << tag_name_ << "': " << e.what() << std::endl;
    return std::vector<std::string>();
  }
}

size_t CTETag::GetBlobSize(const std::string &blob_name) {
  try {
    return static_cast<size_t>(tag_.GetBlobSize(blob_name));
  } catch (const std::exception& e) {
    std::cerr << "CTE GetBlobSize failed for blob '" << blob_name 
              << "' in tag '" << tag_name_ << "': " << e.what() << std::endl;
    return 0;
  }
}

} // namespace coeus

