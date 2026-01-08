#include "hermes_shm/util/real_api.h"

extern "C" {

typedef int (*testfun_t)();

class TestFunApi : public hshm::RealApi {
 public:
  testfun_t testfun_;

  TestFunApi() : hshm::RealApi("testfun", "mylib_intercepted") {
    testfun_ = (testfun_t)dlsym(real_lib_, "testfun");
    REQUIRE_API(testfun_);
  }
};
}
