//
// Created by jaime on 9/5/2023.
//

#ifndef COEUS_TEST_MOCKS_MOCKMPI_H_
#define COEUS_TEST_MOCKS_MOCKMPI_H_
#include <IMPI.h>
#include <gmock/gmock.h>

namespace coeus {

class MockMPI : public IMPI {
 public:
  MOCK_METHOD(bool, isNodeMaster, (), (const, override));
  MOCK_METHOD(adios2::helper::Comm, getNodeCommunicator, (), (const, override));
  MOCK_METHOD(int, getGlobalRank, (), (const, override));
  MOCK_METHOD(int, getGlobalSize, (), (const, override));
  MOCK_METHOD(int, getNodeRank, (), (const, override));
  MOCK_METHOD(int, getNodeSize, (), (const, override));
};

}  // namespace coeus

#endif //COEUS_TEST_MOCKS_MOCKMPI_H_
