#include "basic_test.h"
// All hermes_shm headers are now included via hermes_shm.h in basic_test.h

template <typename AtomicT>
class AtomicTest {
 public:
  AtomicTest() {}

  void Test() {
    // Test arithmetic methods
    PAGE_DIVIDE("Arithmetic") {
      AtomicT atomic = 0;
      REQUIRE(atomic == 0);
      REQUIRE(atomic != 1);
      REQUIRE(atomic + 1 == 1);
      REQUIRE(atomic - 1 == -1);
      atomic += 1;
      REQUIRE(atomic == 1);
      atomic -= 1;
      REQUIRE(atomic == 0);
    }
    // Test constructors
    PAGE_DIVIDE("Constructors") {
      AtomicT atomic = 0;
      REQUIRE(atomic == 0);
      AtomicT atomic2 = atomic;
      REQUIRE(atomic2 == 0);
      AtomicT atomic3(atomic);
      REQUIRE(atomic3 == 0);
      AtomicT atomic4(std::move(atomic));
      REQUIRE(atomic4 == 0);
    }

    // Test assignment operators
    PAGE_DIVIDE("Assignment") {
      AtomicT atomic = 0;
      REQUIRE(atomic == 0);
      AtomicT atomic2;
      atomic2 = atomic;
      REQUIRE(atomic2 == 0);
      AtomicT atomic3;
      atomic3 = std::move(atomic);
      REQUIRE(atomic3 == 0);
    }
  }
};

TEST_CASE("NonAtomic") {
  AtomicTest<hipc::nonatomic<int>> test;
  test.Test();
}

TEST_CASE("Atomic") {
  AtomicTest<hipc::atomic<int>> test;
  test.Test();
}
