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

#include "ITag.h"

namespace coeus {
class IHermes {
 public:
  // Note: I/O is now handled by CTE (Context-Transfer-Engine)
  // Uses CTE Tags (replacing Hermes buckets) and CTE Blobs
  coeus::ITag* tag;
  virtual ~IHermes() = default;

  virtual bool connect() = 0;
  virtual bool GetTag(const std::string &tag_name) = 0;
  virtual bool Put(const std::string &blob_name, size_t blob_size, const void *values) = 0;
  virtual bool Demote(const std::string &tag_name, const std::string &blob_name) = 0;
  virtual bool Prefetch(const std::string &tag_name, const std::string &blob_name) = 0;
};
}
#endif //COEUS_INCLUDE_COMMS_INTERFACES_IHERMES_H_
