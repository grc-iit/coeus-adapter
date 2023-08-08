//
// Created by jaime on 8/7/2023.
//

#ifndef COEUS_INCLUDE_COEUS_COMMS_HERMES_INTERFACES_IBUCKET_H_
#define COEUS_INCLUDE_COEUS_COMMS_HERMES_INTERFACES_IBUCKET_H_
#include <hermes.h>

namespace coeus {
class IBucket {
 public:
  virtual ~IBucket() = default;

  virtual hermes::Status Put(const std::string &blob_name, size_t blob_size, void *values) = 0;
  virtual hermes::Blob Get(const std::string &blob_name) = 0;
  virtual hermes::Blob Get(hermes::BlobId blob_id) override = 0;
  virtual std::vector<hermes::BlobId> GetContainedBlobIds() = 0;
  virtual hermes::Status GetBlobId(const std::string &blob_name, hermes::BlobId &blob_id) = 0;
  virtual std::string GetBlobName(const hermes::BlobId &blob_id) = 0;
};
}
#endif //COEUS_INCLUDE_COEUS_COMMS_HERMES_INTERFACES_IBUCKET_H_
