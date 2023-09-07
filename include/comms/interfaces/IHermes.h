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

#ifndef COEUS_INCLUDE_COMMS_INTERFACES_IHERMES_H_
#define COEUS_INCLUDE_COMMS_INTERFACES_IHERMES_H_
#include <hermes.h>

#include "IBucket.h"

namespace coeus {
class IHermes {
 public:
  std::unique_ptr<hermes::Hermes> hermes;
  virtual ~IHermes() = default;

  virtual bool connect() = 0;
  virtual std::unique_ptr<IBucket> GetBucket(const std::string &bucket_name) = 0;
};
}
#endif //COEUS_INCLUDE_COMMS_INTERFACES_IHERMES_H_
