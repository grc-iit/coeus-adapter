//
// Created by jaime on 8/7/2023.
//

#ifndef COEUS_INCLUDE_COEUS_BUCKET_H_
#define COEUS_INCLUDE_COEUS_BUCKET_H_
#include <hermes.h>
#include "interfaces/IBucket.h"
#include "interfaces/IHermes.h"

namespace coeus{
class Bucket : public IBucket {
 public:
  hapi::Bucket bkt;

  Bucket(const std::string &bucket_name, coeus::IHermes *h) {
    bkt = h->hermes->GetBucket(bucket_name);
  }

  hermes::Status Put(const std::string &blob_name, size_t blob_size, void* values) override {
    hapi::Context ctx;
    hermes::Blob blob(blob_size);
    hermes::BlobId blob_id;
    memcpy(blob.data(), values, blob_size);
    bkt.Put(blob_name, blob, blob_id, ctx);
  };

  hermes::Blob Get(const std::string &blob_name) override {
    hermes::BlobId blob_id;
    bkt.GetBlobId(blob_name, blob_id);
    return Get(blob_id);
  };

  hermes::Blob Get(hermes::BlobId blob_id) override {
    hapi::Context ctx;
    hermes::Blob blob;
    bkt.Get(blob_id, blob, ctx);
    return blob;
  };

  std::vector<hermes::BlobId> GetContainedBlobIds() override {
    return bkt.GetContainedBlobIds();
  }

  hermes::Status GetBlobId(const std::string &blob_name, hermes::BlobId &blob_id) override {
    return bkt.GetBlobId(blob_name, blob_id);
  }

  std::string GetBlobName(const hermes::BlobId &blob_id) override {
    return bkt.GetBlobName(blob_id);
  }
};

}
#endif //COEUS_INCLUDE_COEUS_BUCKET_H_
