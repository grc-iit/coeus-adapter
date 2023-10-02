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

#ifndef COEUS_INCLUDE_COMMS_INTERFACES_IMPI_H_
#define COEUS_INCLUDE_COMMS_INTERFACES_IMPI_H_

namespace coeus {

class IMPI {
 public:
  virtual ~IMPI() = default;
  virtual bool isNodeMaster() const = 0;
  virtual int getGlobalRank() const = 0;
  virtual int getGlobalSize() const = 0;
  virtual int getNodeRank() const = 0;
  virtual int getNodeSize() const = 0;
};

} // namespace coeus
#endif //COEUS_INCLUDE_COMMS_INTERFACES_IMPI_H_
