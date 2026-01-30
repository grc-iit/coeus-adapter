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

#include "comms/CTETagClient.h"

#include <cstring>
#include <stdexcept>
#include <iostream>

namespace coeus {

CTETagClient::CTETagClient(wrp_cte::core::Client* cte_client, const std::string& tag_name)
    : tag_name_(tag_name) {
  if (cte_client) {
    cte_client_ = cte_client;
  } else {
    cte_client_ = WRP_CTE_CLIENT;
  }

  if (!cte_client_) {
    throw std::runtime_error("CTE client not initialized. Call WRP_CTE_CLIENT_INIT() first.");
  }

  name = tag_name;

  // Get or create tag using synchronous Client API
  tag_id_ = cte_client_->GetOrCreateTag(tag_name);
}

CTETagClient::CTETagClient(wrp_cte::core::Client* cte_client,
                           const wrp_cte::core::TagId& tag_id,
                           const std::string& tag_name)
    : tag_name_(tag_name), tag_id_(tag_id) {
  if (cte_client) {
    cte_client_ = cte_client;
  } else {
    cte_client_ = WRP_CTE_CLIENT;
  }

  if (!cte_client_) {
    throw std::runtime_error("CTE client not initialized. Call WRP_CTE_CLIENT_INIT() first.");
  }

  name = tag_name;
}

void CTETagClient::Put(const std::string &blob_name, size_t blob_size, const void* values) {
  try {
    // Allocate shared memory for the data
    auto *ipc_manager = CHI_IPC;
    hipc::FullPtr<char> shm_fullptr = ipc_manager->AllocateBuffer(blob_size);
    
    if (shm_fullptr.IsNull()) {
      throw std::runtime_error("Failed to allocate shared memory for PutBlob");
    }
    
    // Copy data to shared memory
    memcpy(shm_fullptr.ptr_, values, blob_size);
    
    // Convert to hipc::ShmPtr<> for API call
    hipc::ShmPtr<> shm_ptr(shm_fullptr.shm_);

    // Synchronous PutBlob (blocks until done; throws on failure)
    cte_client_->PutBlob(tag_id_, blob_name, 0, blob_size, shm_ptr,
                         GetDefaultBlobScore(), 0);

    // Free shared memory buffer
    ipc_manager->FreeBuffer(shm_fullptr);
  } catch (const std::exception& e) {
    std::cerr << "CTE PutBlob failed for blob '" << blob_name 
              << "' in tag '" << tag_name_ << "': " << e.what() << std::endl;
    throw std::runtime_error("CTE PutBlob failed: " + std::string(e.what()));
  }
}

std::vector<uint8_t> CTETagClient::Get(const std::string &blob_name) {
  try {
    // Get blob size first
    auto size_task = cte_client_->AsyncGetBlobSize(tag_id_, blob_name);
    size_task.Wait();
    
    if (size_task->GetReturnCode() != 0) {
      // Blob doesn't exist
      return std::vector<uint8_t>();
    }
    
    chi::u64 blob_size = size_task->size_;
    if (blob_size == 0) {
      return std::vector<uint8_t>();
    }
    
    // Allocate shared memory for reading
    auto *ipc_manager = CHI_IPC;
    hipc::FullPtr<char> shm_buffer = ipc_manager->AllocateBuffer(blob_size);
    
    if (shm_buffer.IsNull()) {
      throw std::runtime_error("Failed to allocate shared memory for GetBlob");
    }
    
    // Convert to hipc::ShmPtr<> for API call
    hipc::ShmPtr<> shm_ptr(shm_buffer.shm_);
    
    // Call async GetBlob and wait for completion
    auto task = cte_client_->AsyncGetBlob(tag_id_, blob_name, 0, blob_size, 0, shm_ptr);
    task.Wait();
    
    if (task->GetReturnCode() != 0) {
      ipc_manager->FreeBuffer(shm_buffer);
      return std::vector<uint8_t>(); // Blob not found or error
    }
    
    // Copy data from shared memory to vector
    std::vector<uint8_t> buffer(blob_size);
    memcpy(buffer.data(), shm_buffer.ptr_, blob_size);
    
    // Free shared memory buffer
    ipc_manager->FreeBuffer(shm_buffer);
    
    return buffer;
  } catch (const std::exception& e) {
    std::cerr << "CTE GetBlob failed for blob '" << blob_name 
              << "' in tag '" << tag_name_ << "': " << e.what() << std::endl;
    return std::vector<uint8_t>(); // Return empty vector on error
  }
}

std::vector<std::string> CTETagClient::GetContainedBlobNames() {
  try {
    auto task = cte_client_->AsyncGetContainedBlobs(tag_id_);
    task.Wait();
    
    if (task->GetReturnCode() != 0) {
      return std::vector<std::string>();
    }
    
    // blob_names_ is already std::vector<std::string>
    return task->blob_names_;
  } catch (const std::exception& e) {
    std::cerr << "CTE GetContainedBlobs failed for tag '" 
              << tag_name_ << "': " << e.what() << std::endl;
    return std::vector<std::string>();
  }
}

size_t CTETagClient::GetBlobSize(const std::string &blob_name) {
  try {
    auto task = cte_client_->AsyncGetBlobSize(tag_id_, blob_name);
    task.Wait();
    
    if (task->GetReturnCode() != 0) {
      return 0;
    }
    
    return static_cast<size_t>(task->size_);
  } catch (const std::exception& e) {
    std::cerr << "CTE GetBlobSize failed for blob '" << blob_name 
              << "' in tag '" << tag_name_ << "': " << e.what() << std::endl;
    return 0;
  }
}

} // namespace coeus

