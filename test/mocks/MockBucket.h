//
// Created by jaime on 8/7/2023.
//

#ifndef COEUS_TEST_MOCKS_MOCKBUCKET_H_
#define COEUS_TEST_MOCKS_MOCKBUCKET_H_

#include <IBucket.h>
#include <gmock/gmock.h>

namespace coeus {

class MockIBucket : public IBucket {
 public:
  MOCK_METHOD(hermes::BlobId, Put, (const std::string &blob_name, size_t blob_size, void *values), (override));
  MOCK_METHOD(hermes::Blob, Get, (const std::string &blob_name), (override));
  MOCK_METHOD(hermes::Blob, Get, (hermes::BlobId blob_id), (override));
  MOCK_METHOD(std::vector<hermes::BlobId>, GetContainedBlobIds, (), (override));
  MOCK_METHOD(hermes::BlobId, GetBlobId, (const std::string &blob_name), (override));
  MOCK_METHOD(std::string, GetBlobName, (const hermes::BlobId &blob_id), (override));
};
} // namespace coeus

#endif //COEUS_TEST_MOCKS_MOCKBUCKET_H_
