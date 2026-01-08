#include "basic_test.h"
#include "my_lib1.h"
#include "my_lib2.h"
#include "singleton_lib.h"

TEST_CASE("TestSingleton") {
  MyLib1 p1;
  MyLib2 p2;
  std::string p1_val("p1");
  std::string p2_val("p2");
  p1.SetSingleton(p1_val);
  p2.SetSingleton(p2_val);
  REQUIRE(p1.GetSingleton() == p2_val);
  REQUIRE(p2.GetSingleton() == p2_val);
  REQUIRE(hshm::Singleton<MyStruct>::GetInstance()->string_ == p2_val);
}

TEST_CASE("TestGlobalSingleton") {
  MyLib1 p1;
  MyLib2 p2;
  std::string p1_val("p1");
  std::string p2_val("p2");
  p1.SetGlobalSingleton(p1_val);
  p2.SetGlobalSingleton(p2_val);
  REQUIRE(p1.GetGlobalSingleton() == p2_val);
  REQUIRE(p2.GetGlobalSingleton() == p2_val);
  REQUIRE(hshm::GlobalSingleton<MyStruct>::GetInstance()->string_ == p2_val);
}
