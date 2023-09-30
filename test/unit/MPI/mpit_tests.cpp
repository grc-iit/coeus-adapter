//
// Created by jaime on 9/5/2023.
//

#include "adios2/helper/adiosComm.h"
#include "comms/MPI.h"
#include <gtest/gtest.h>

class MPITest : public ::testing::Test {
 protected:
  adios2::helper::Comm globalComm;
  std::shared_ptr<coeus::MPI> mpiInstance;

  virtual void SetUp() {
    // Initialize the global communicator
    globalComm = globalComm.World();
    mpiInstance = std::make_shared<coeus::MPI>(globalComm.Duplicate());
    
  }

  virtual void TearDown() {
    // Cleanup if necessary
  }
};

TEST_F(MPITest, TestGlobalRank) {
int rank = mpiInstance->getGlobalRank();
EXPECT_GE(rank, 0);
EXPECT_LT(rank, mpiInstance->getGlobalSize());
}

TEST_F(MPITest, TestGlobalSize) {
int size = mpiInstance->getGlobalSize();
EXPECT_GT(size, 0);
}

TEST_F(MPITest, TestNodeRank) {

int rank = mpiInstance->getNodeRank();
EXPECT_GE(rank, 0);
EXPECT_LT(rank, mpiInstance->getNodeSize());
}

TEST_F(MPITest, TestNodeSize) {

int size = mpiInstance->getNodeSize();
EXPECT_GT(size, 0);
}

TEST_F(MPITest, TestIsNodeMaster) {
  bool isMaster = mpiInstance->isNodeMaster();
  if (mpiInstance->getNodeRank() == 0) {
  EXPECT_TRUE(isMaster);
  } else {
  EXPECT_FALSE(isMaster);
  }
}

