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

#ifndef COEUS_INCLUDE_COMMS_INTERFACES_IBUCKET_H_
#define COEUS_INCLUDE_COMMS_INTERFACES_IBUCKET_H_
#include <hermes/hermes_types.h>

namespace coeus {
class IBucket {
 public:
  std::string name;
  virtual ~IBucket() = default;

  virtual void Put(const std::string &blob_name, size_t blob_size, const void *values) = 0;
  virtual hermes::Blob Get(const std::string &blob_name) = 0;
  virtual hermes::Blob Get(hermes::BlobId blob_id) = 0;
  virtual std::vector<hermes::BlobId> GetContainedBlobIds() = 0;
  virtual hermes::BlobId GetBlobId(const std::string &blob_name) = 0;
  virtual std::string GetBlobName(const hermes::BlobId &blob_id) = 0;
};
}
#endif //COEUS_INCLUDE_COMMS_INTERFACES_IBUCKET_H_
