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

#ifndef COEUS_INCLUDE_COMMS_BUCKET_H_
#define COEUS_INCLUDE_COMMS_BUCKET_H_
#include <hermes/bucket.h>
#include "interfaces/IBucket.h"
#include "interfaces/IHermes.h"
#include "common/Tracer.h"

namespace coeus{
class Bucket : public IBucket {
 public:
  hapi::Bucket bkt;

  Bucket(const std::string &bucket_name, coeus::IHermes *h) {
    name = bucket_name;
    bkt = h->hermes->GetBucket(bucket_name);
  }

//  void SwitchBucket(const std::string &bucket_name, coeus::IHermes *h){
//      bkt = h->hermes->GetBucket(bucket_name);
//  }
//  hermes::BlobId Put(const std::string &blob_name, const std::string &data){
//    hapi::Context ctx;
//    bkt.Put<std::string>(blob_name, data, ctx);
//  }

  void Put(const std::string &blob_name, size_t blob_size, const void* values) override {
    hapi::Context ctx;
    hermes::Blob blob(blob_size);
    hermes::BlobId blob_id;
    memcpy(blob.data(), values, blob_size);
    bkt.AsyncPut(blob_name, blob, ctx);
  };

//  void Put_hash(const std::string &blob_name, size_t blob_size, const void* values) override {
//      TRACE_FUNC(blob_name);
//      hapi::Context ctx;
//      hermes::Blob blob(blob_size);
//      hermes::BlobId blob_id;
//      memcpy(blob.data(), values, blob_size);
//      bkt.AsyncPut(blob_name, blob, ctx);
//  };
  hermes::Blob Get(const std::string &blob_name) override {
    hapi::Context ctx;
    hermes::Blob blob;
    bkt.Get(blob_name, blob, ctx);
    return blob;
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

  hermes::BlobId GetBlobId(const std::string &blob_name) override {
    return bkt.GetBlobId(blob_name);
  }

  std::string GetBlobName(const hermes::BlobId &blob_id) override {
    return bkt.GetBlobName(blob_id);
  }
};

}
#endif //COEUS_INCLUDE_COMMS_BUCKET_H_
