//
// Created by jaime on 8/7/2023.
//

#ifndef COEUS_TEST_MOCKS_MOCKTAG_H_
#define COEUS_TEST_MOCKS_MOCKTAG_H_

#include <comms/interfaces/ITag.h>
#include <gmock/gmock.h>
#include <vector>
#include <cstdint>

namespace coeus {

class MockITag : public ITag {
 public:
  MOCK_METHOD(void, Put, (const std::string &blob_name, size_t blob_size, const void *values), (override));
  MOCK_METHOD(std::vector<uint8_t>, Get, (const std::string &blob_name), (override));
  MOCK_METHOD(std::vector<std::string>, GetContainedBlobNames, (), (override));
  MOCK_METHOD(size_t, GetBlobSize, (const std::string &blob_name), (override));
};
} // namespace coeus

#endif //COEUS_TEST_MOCKS_MOCKBUCKET_H_
