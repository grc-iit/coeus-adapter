//
// Created by jaime on 8/5/2023.
//

#ifndef COEUS_TEST_MOCKS_MOCKHERMES_H_
#define COEUS_TEST_MOCKS_MOCKHERMES_H_

#include <gmock/gmock.h>
#include <IHermes.h>

namespace coeus::testing {
class MockHermes : public coeus::IHermes {
 public:
  MOCK_METHOD(bool, connect, (), (override));
  MOCK_METHOD(std::unique_ptr<IBucket>, GetBucket, (const std::string &bucket_name), (override));
};
}
#endif //COEUS_TEST_MOCKS_MOCKHERMES_H_
