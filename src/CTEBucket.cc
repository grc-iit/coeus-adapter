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

#include "comms/CTEBucket.h"
#include <cstring>
#include <stdexcept>
#include <functional>
#include <iostream>

namespace coeus {

CTEBucket::CTEBucket(const std::string &bucket_name) 
    : bucket_name_(bucket_name), tag_(bucket_name) {
  name = bucket_name;
  
  // Ensure CTE client is initialized
  if (!WRP_CTE_CLIENT) {
    throw std::runtime_error("CTE client not initialized. Call WRP_CTE_CLIENT_INIT() first.");
  }
}

void CTEBucket::Put(const std::string &blob_name, size_t blob_size, const void* values) {
  try {
    // CTE Tag::PutBlob handles shared memory automatically for sync operations
    // Uses default score for data placement
    tag_.PutBlob(blob_name, static_cast<const char*>(values), blob_size, 0);
    
    // Update blob ID mapping for compatibility
    UpdateBlobMapping(blob_name);
  } catch (const std::exception& e) {
    std::cerr << "CTE PutBlob failed for blob '" << blob_name 
              << "' in bucket '" << bucket_name_ << "': " << e.what() << std::endl;
    throw std::runtime_error("CTE PutBlob failed: " + std::string(e.what()));
  }
}

hermes::Blob CTEBucket::Get(const std::string &blob_name) {
  try {
    // Get blob size first
    chi::u64 blob_size = tag_.GetBlobSize(blob_name);
    if (blob_size == 0) {
      // Blob doesn't exist or is empty
      return hermes::Blob();
    }
    
    // Allocate buffer for data
    std::vector<char> buffer(blob_size);
    
    // Retrieve blob data
    tag_.GetBlob(blob_name, buffer.data(), blob_size, 0);
    
    // Convert to hermes::Blob for interface compatibility
    hermes::Blob blob(blob_size);
    memcpy(blob.data(), buffer.data(), blob_size);
    
    // Update blob ID mapping
    UpdateBlobMapping(blob_name);
    
    return blob;
  } catch (const std::exception& e) {
    std::cerr << "CTE GetBlob failed for blob '" << blob_name 
              << "' in bucket '" << bucket_name_ << "': " << e.what() << std::endl;
    return hermes::Blob(); // Return empty blob on error
  }
}

hermes::Blob CTEBucket::Get(hermes::BlobId blob_id) {
  // Look up blob name from ID
  std::lock_guard<std::mutex> lock(mapping_mutex_);
  auto it = blob_id_to_name_.find(blob_id);
  if (it == blob_id_to_name_.end()) {
    // Blob ID not found in mapping
    return hermes::Blob();
  }
  
  // Get blob by name
  return Get(it->second);
}

std::vector<hermes::BlobId> CTEBucket::GetContainedBlobIds() {
  try {
    // Get all blob names from CTE tag
    std::vector<std::string> blob_names = tag_.GetContainedBlobs();
    
    std::vector<hermes::BlobId> blob_ids;
    blob_ids.reserve(blob_names.size());
    
    std::lock_guard<std::mutex> lock(mapping_mutex_);
    for (const auto& blob_name : blob_names) {
      // Generate or retrieve blob ID
      hermes::BlobId blob_id = GenerateBlobId(blob_name);
      blob_ids.push_back(blob_id);
      
      // Update mapping
      blob_name_to_id_[blob_name] = blob_id;
      blob_id_to_name_[blob_id] = blob_name;
    }
    
    return blob_ids;
  } catch (const std::exception& e) {
    std::cerr << "CTE GetContainedBlobs failed for bucket '" 
              << bucket_name_ << "': " << e.what() << std::endl;
    return std::vector<hermes::BlobId>();
  }
}

hermes::BlobId CTEBucket::GetBlobId(const std::string &blob_name) {
  std::lock_guard<std::mutex> lock(mapping_mutex_);
  
  // Check if mapping exists
  auto it = blob_name_to_id_.find(blob_name);
  if (it != blob_name_to_id_.end()) {
    return it->second;
  }
  
  // Generate new ID and update mapping
  hermes::BlobId blob_id = GenerateBlobId(blob_name);
  blob_name_to_id_[blob_name] = blob_id;
  blob_id_to_name_[blob_id] = blob_name;
  
  return blob_id;
}

std::string CTEBucket::GetBlobName(const hermes::BlobId &blob_id) {
  std::lock_guard<std::mutex> lock(mapping_mutex_);
  
  auto it = blob_id_to_name_.find(blob_id);
  if (it != blob_id_to_name_.end()) {
    return it->second;
  }
  
  // Blob ID not found
  return "";
}

hermes::BlobId CTEBucket::GenerateBlobId(const std::string &blob_name) {
  // Generate a deterministic BlobId from blob name using hash
  // This maintains compatibility with Hermes interface
  std::hash<std::string> hasher;
  size_t hash_value = hasher(bucket_name_ + ":" + blob_name);
  
  hermes::BlobId blob_id;
  // Use hash to create a deterministic ID
  // Hermes BlobId structure typically has major_ and minor_ fields
  // If the structure is different, this will need to be adjusted
  blob_id.major_ = static_cast<uint64_t>(hash_value);
  blob_id.minor_ = static_cast<uint64_t>(hash_value >> 32);
  
  return blob_id;
}

void CTEBucket::UpdateBlobMapping(const std::string &blob_name) {
  std::lock_guard<std::mutex> lock(mapping_mutex_);
  
  // Only update if not already in mapping
  if (blob_name_to_id_.find(blob_name) == blob_name_to_id_.end()) {
    hermes::BlobId blob_id = GenerateBlobId(blob_name);
    blob_name_to_id_[blob_name] = blob_id;
    blob_id_to_name_[blob_id] = blob_name;
  }
}

} // namespace coeus

