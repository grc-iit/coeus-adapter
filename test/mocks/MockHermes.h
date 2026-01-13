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
  MOCK_METHOD(bool, GetTag, (const std::string &tag_name), (override));
  MOCK_METHOD(bool, Demote, (const std::string &tag_name, const std::string &blob_name), (override));
  MOCK_METHOD(bool, Prefetch, (const std::string &tag_name, const std::string &blob_name), (override));
};
}
#endif //COEUS_TEST_MOCKS_MOCKHERMES_H_
