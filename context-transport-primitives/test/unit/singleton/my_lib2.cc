#include "my_lib2.h"

#include "singleton_lib.h"

void MyLib2::SetEasySingleton(const std::string &value) {
  hshm::Singleton<MyStruct>::GetInstance()->string_ = value;
}

std::string MyLib2::GetEasySingleton() {
  return hshm::Singleton<MyStruct>::GetInstance()->string_;
}

void MyLib2::SetEasyGlobalSingleton(const std::string &value) {
  hshm::GlobalSingleton<MyStruct>::GetInstance()->string_ = value;
}

std::string MyLib2::GetEasyGlobalSingleton() {
  return hshm::GlobalSingleton<MyStruct>::GetInstance()->string_;
}

void MyLib2::SetSingleton(const std::string &value) {
  hshm::Singleton<MyStruct>::GetInstance()->string_ = value;
}

std::string MyLib2::GetSingleton() {
  return hshm::Singleton<MyStruct>::GetInstance()->string_;
}

void MyLib2::SetGlobalSingleton(const std::string &value) {
  hshm::GlobalSingleton<MyStruct>::GetInstance()->string_ = value;
}

std::string MyLib2::GetGlobalSingleton() {
  return hshm::GlobalSingleton<MyStruct>::GetInstance()->string_;
}
