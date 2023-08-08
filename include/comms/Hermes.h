//
// Created by jaime on 8/5/2023.
//

#ifndef COEUS_INCLUDE_COEUS_HERMES_H_
#define COEUS_INCLUDE_COEUS_HERMES_H_

#include "interfaces/IHermes.h"

namespace coeus {
class Hermes : public IHermes {
 public:
  Hermes() = default;

  std::unique_ptr<hermes::Hermes> connect() override {
    auto hermes_ptr = hapi::Hermes::Create(hermes::HermesType::kClient);
    return std::unique_ptr<hermes::Hermes>(hermes_ptr);
  };

  std::unique_ptr<IBucket> GetBucket(const std::string &bucket_name) override {
    return std::make_unique<coeus::Bucket>(bucket_name, this);
  }
};

}
#endif //COEUS_INCLUDE_COEUS_HERMES_H_
