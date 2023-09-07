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

namespace coeus {
class Hermes : public IHermes {
 public:
  Hermes() = default;

  bool connect() override {
    auto hermes_ptr = hapi::Hermes::Create(hermes::HermesType::kClient);
    hermes = std::unique_ptr<hermes::Hermes>(hermes_ptr);
    return hermes->IsInitialized();
  };

  std::unique_ptr<IBucket> GetBucket(const std::string &bucket_name) override {
    return std::make_unique<coeus::Bucket>(bucket_name, this);
  }
};

}
#endif //COEUS_INCLUDE_COMMS_HERMES_H_
