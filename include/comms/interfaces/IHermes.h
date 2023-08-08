//
// Created by jaime on 8/7/2023.
//

#ifndef COEUS_INCLUDE_COEUS_COMMS_HERMES_INTERFACES_IHERMES_H_
#define COEUS_INCLUDE_COEUS_COMMS_HERMES_INTERFACES_IHERMES_H_
#include <hermes.h>

#include "IBucket.h"

namespace coeus {
class IHermes {
 public:
  std::unique_ptr<hermes::Hermes> hermes;
  virtual ~IHermes() = default;

  virtual std::unique_ptr<hermes::Hermes> connect() = 0;
  virtual std::unique_ptr<IBucket> GetBucket(const std::string &bucket_name) = 0;
};
}
#endif //COEUS_INCLUDE_COEUS_COMMS_HERMES_INTERFACES_IHERMES_H_
