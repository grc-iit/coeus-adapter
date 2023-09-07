/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Distributed under BSD 3-Clause license.                                   *
 * Copyright by the Illinois Institute of Technology.                        *
 * All rights reserved.                                                      *
 *                                                                           *
 * This file is part of Coeus-adapter. The full Coeus-adapter copyright      *
 * notice, including terms governing use, modification, and redistribution,  *
 * is contained in the COPYING file, which can be found at the top directory.*
 * If you do not have access to the file, you may request a copy             *
 * from scslab@iit.edu.                                                      *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef COEUS_INCLUDE_COMMON_ERRORDEFINITION_H
#define COEUS_INCLUDE_COMMON_ERRORDEFINITION_H

#include <tuple>
#include <string>
#include <exception>
#include <cstddef>
#include <utility>

template <class Tup, class Func, std::size_t ...Is>
constexpr void static_for_impl(Tup&& t, Func &&f, std::index_sequence<Is...> )
{
  ( f(std::integral_constant<std::size_t, Is>{}, std::get<Is>(t)),... );
}

template <class ... T, class Func >
constexpr void static_for(std::tuple<T...>&t, Func &&f)
{
  static_for_impl(t, std::forward<Func>(f), std::make_index_sequence<sizeof...(T)>{});
}


namespace coeus::common {
typedef struct ErrorCode {
 private:
  int code_;
  std::string message_;

 public:
  ErrorCode(int code, std::string message) : code_(code), message_(message) {}

  int getCode() const { return code_; }
  std::string getMessage() const { return message_; }
  void setMessage(std::string message) { message_ = message; }
} ErrorCode;

class ErrorException : public std::exception {
 private:
  bool replace(std::string &str, const std::string &from, const std::string &to) {
    size_t start_pos = str.find(from);
    if (start_pos == std::string::npos)
      return false;
    str.replace(start_pos, from.length(), to);
    return true;
  }
 public:
  template<typename... Args>
  ErrorException(ErrorCode errorcode, Args... args): errorCode_(errorcode) {

    std::string error_message_ = std::to_string(errorCode_.getCode());
    error_message_ += "\t";
    error_message_ += errorCode_.getMessage();
    auto args_obj = std::make_tuple(args...);
    const ulong args_size = std::tuple_size<decltype(args_obj)>::value;

    if (args_size != 0) {
      static_for(args_obj, [&](auto i, auto w) {
        replace(error_message_, "%s", w);
      });
      errorCode_.setMessage(error_message_);
    }

  }

  const char *what() const throw() {
    perror(errorCode_.getMessage().c_str());
    return errorCode_.getMessage().c_str();
  }

 private:
  ErrorCode errorCode_;
};
}
#endif //COEUS_INCLUDE_COMMON_ERRORDEFINITION_H

