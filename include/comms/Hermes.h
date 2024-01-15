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

#ifndef COEUS_INCLUDE_COMMS_HERMES_H_
#define COEUS_INCLUDE_COMMS_HERMES_H_

#include "interfaces/IHermes.h"
#include "Bucket.h"
#include "common/Tracer.h"

namespace coeus {
class Hermes : public IHermes {
 public:
  float promote_weight = 0.5;
  float demote_weight = -0.5;

  Hermes() = default;

  bool connect() override {
      TRACE_FUNC();
    std::cout << "Entering connect" << std::endl;
    std::cout << "HERMES_CONF: " << getenv("HERMES_CONF") << std::endl;
    TRANSPARENT_HERMES();
    std::cout << "transparent done" << std::endl;
    hermes = HERMES;
    std::cout << "hermes assigment" << std::endl;
    HRUN_ADMIN->RegisterTaskLibRoot(hrun::DomainId::GetGlobal(), "coeus_mdm");
    std::cout << "Registered task" << std::endl;
    return hermes->IsInitialized();
  };

    bool GetBucket(const std::string &bucket_name) override {
        TRACE_FUNC();
    bkt = (IBucket*) new coeus::Bucket(bucket_name, this);
    return true;
  }

  bool Demote(const std::string &bucket_name, const std::string &blob_name) override {
      TRACE_FUNC();
    hapi::Context ctx;
    auto bkt = hermes->GetBucket(bucket_name);

    hermes::BlobId blob_id = bkt.GetBlobId(blob_name);
    float blob_score = bkt.GetBlobScore(blob_id);

    bkt.ReorganizeBlob(blob_id, blob_score + demote_weight, 0, ctx);
  }

  bool Prefetch(const std::string &bucket_name, const std::string &blob_name) override {
      TRACE_FUNC();
    hapi::Context ctx;
    auto bkt = hermes->GetBucket(bucket_name);

    hermes::BlobId blob_id = bkt.GetBlobId(blob_name);
    float blob_score = bkt.GetBlobScore(blob_id);

    bkt.ReorganizeBlob(blob_id, blob_score + promote_weight, 0, ctx);
  }
};

}
#endif //COEUS_INCLUDE_COMMS_HERMES_H_
