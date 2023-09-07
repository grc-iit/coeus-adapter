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

#ifndef COEUS_INCLUDE_COMMS_INTERFACES_MPI_H_
#define COEUS_INCLUDE_COMMS_INTERFACES_MPI_H_

#include <unistd.h>
#include <functional>
#include <string>

#include <mpi.h>
#include "adios2/helper/adiosComm.h"

#include "interfaces/IMPI.h"

namespace coeus {
class MPI : public IMPI {
 private:
  adios2::helper::Comm globalComm;
  int nodeId;
  adios2::helper::Comm nodeComm;
  int nodeMasterRank;

  int getNodeID() {
    char hostname[256];
    gethostname(hostname, sizeof(hostname));

    // Hash the hostname to produce a unique integer ID for each node
    std::hash<std::string> hasher;
    return hasher(std::string(hostname));
  }

 public:
  MPI(const adios2::helper::Comm& globalComm)
      : globalComm(globalComm), nodeId(getNodeID()) {
    createNodeCommunicator();
    defineNodeMaster();
  }

  void createNodeCommunicator() {
    // Split the global communicator based on the node ID
    nodeComm = globalComm.Split(nodeId, globalComm.Rank());
  }

  void defineNodeMaster() {
    // Define the node master (or root) as the process with the lowest rank in the node communicator
    nodeMasterRank = 0;  // Assuming rank 0 is the master for each node communicator
  }

  bool isNodeMaster() const override {
    return nodeComm.Rank() == nodeMasterRank;
  }

  int getGlobalRank() const override {
    return globalComm.Rank();
  }

  int getGlobalSize() const override {
    return globalComm.Size();
  }

  int getNodeRank() const override {
    return nodeComm.Rank();
  }

  int getNodeSize() const override {
    return nodeComm.Size();
  }
};
}
#endif //COEUS_INCLUDE_COMMS_INTERFACES_MPI_H_
