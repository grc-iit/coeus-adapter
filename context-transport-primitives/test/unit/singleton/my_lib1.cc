#include "my_lib1.h"

#include "singleton_lib.h"

void MyLib1::SetEasySingleton(const std::string &value) {
  hshm::Singleton<MyStruct>::GetInstance()->string_ = value;
}

std::string MyLib1::GetEasySingleton() {
  return hshm::Singleton<MyStruct>::GetInstance()->string_;
}

void MyLib1::SetEasyGlobalSingleton(const std::string &value) {
  hshm::GlobalSingleton<MyStruct>::GetInstance()->string_ = value;
}

std::string MyLib1::GetEasyGlobalSingleton() {
  return hshm::GlobalSingleton<MyStruct>::GetInstance()->string_;
}

void MyLib1::SetSingleton(const std::string &value) {
  hshm::Singleton<MyStruct>::GetInstance()->string_ = value;
}

std::string MyLib1::GetSingleton() {
  return hshm::Singleton<MyStruct>::GetInstance()->string_;
}

void MyLib1::SetGlobalSingleton(const std::string &value) {
  hshm::GlobalSingleton<MyStruct>::GetInstance()->string_ = value;
}

std::string MyLib1::GetGlobalSingleton() {
  return hshm::GlobalSingleton<MyStruct>::GetInstance()->string_;
}
