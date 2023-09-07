//
// Created by jaime on 9/5/2023.
//

#include "adios2/helper/adiosComm.h"
#include "comms/MPI.h"
#include <gtest/gtest.h>

namespace coeus {

class MPITest : public ::testing::Test {
 protected:
  adios2::helper::Comm globalComm;

  virtual void SetUp() {
    // Initialize the global communicator
    globalComm = globalComm.World();
  }

  virtual void TearDown() {
    // Cleanup if necessary
  }
};

TEST_F(MPITest, TestGlobalRank) {
MPI mpiInstance(globalComm);
int rank = mpiInstance.getGlobalRank();
EXPECT_GE(rank, 0);
EXPECT_LT(rank, mpiInstance.getGlobalSize());
}

TEST_F(MPITest, TestGlobalSize) {
MPI mpiInstance(globalComm);
int size = mpiInstance.getGlobalSize();
EXPECT_GT(size, 0);
}

TEST_F(MPITest, TestNodeRank) {
MPI mpiInstance(globalComm);
int rank = mpiInstance.getNodeRank();
EXPECT_GE(rank, 0);
EXPECT_LT(rank, mpiInstance.getNodeSize());
}

TEST_F(MPITest, TestNodeSize) {
MPI mpiInstance(globalComm);
int size = mpiInstance.getNodeSize();
EXPECT_GT(size, 0);
}

TEST_F(MPITest, TestIsNodeMaster) {
MPI mpiInstance(globalComm);
bool isMaster = mpiInstance.isNodeMaster();
if (mpiInstance.getNodeRank() == 0) {
EXPECT_TRUE(isMaster);
} else {
EXPECT_FALSE(isMaster);
}
}

}  // namespace coeus

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
