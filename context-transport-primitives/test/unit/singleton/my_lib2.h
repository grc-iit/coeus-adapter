#ifndef MY_LIB2_H
#define MY_LIB2_H

#include <string>

class MyLib2 {
 public:
  std::string GetEasySingleton();
  void SetEasySingleton(const std::string& value);

  std::string GetEasyGlobalSingleton();
  void SetEasyGlobalSingleton(const std::string& value);

  std::string GetSingleton();
  void SetSingleton(const std::string& value);

  std::string GetGlobalSingleton();
  void SetGlobalSingleton(const std::string& value);
};

#endif  // MY_LIB2_H
