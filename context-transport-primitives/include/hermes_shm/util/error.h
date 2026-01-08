/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Distributed under BSD 3-Clause license.                                   *
 * Copyright by The HDF Group.                                               *
 * Copyright by the Illinois Institute of Technology.                        *
 * All rights reserved.                                                      *
 *                                                                           *
 * This file is part of Hermes. The full Hermes copyright notice, including  *
 * terms governing use, modification, and redistribution, is contained in    *
 * the COPYING file, which can be found at the top directory. If you do not  *
 * have access to the file, you may request a copy from help@hdfgroup.org.   *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef HSHM_ERROR_H
#define HSHM_ERROR_H

// #ifdef __cplusplus

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

#include "hermes_shm/util/formatter.h"

#define HSHM_ERROR_TYPE std::shared_ptr<hshm::Error>
#define HSHM_ERROR_HANDLE_START() try {
#define HSHM_ERROR_HANDLE_END()         \
  }                                     \
  catch (HSHM_ERROR_TYPE & err) {       \
    err->print();                       \
    exit(-1024);                        \
  }                                     \
  catch (std::exception & e) {          \
    std::cerr << e.what() << std::endl; \
    exit(-1024);                        \
  }
#define HSHM_ERROR_HANDLE_TRY try
#define HSHM_ERROR_PTR err
#define HSHM_ERROR_HANDLE_CATCH catch (HSHM_ERROR_TYPE & HSHM_ERROR_PTR)
#define HSHM_ERROR_IS(err, check) (err->get_code() == check.get_code())

#if HSHM_IS_HOST
#if __HIP_DEVICE_COMPILE__
#error "Why??"
#endif
#define HSHM_THROW_ERROR(CODE, ...) throw CODE.format(__VA_ARGS__)
#define HSHM_THROW_STD_ERROR(...) throw std::runtime_error(__VA_ARGS__);
#endif

#if HSHM_IS_GPU
#define HSHM_THROW_ERROR(CODE, ...)
#define HSHM_THROW_STD_ERROR(...)
#endif

namespace hshm {

class Error : std::exception {
 private:
  const char* fmt_;
  std::string msg_;

 public:
  Error() : fmt_() {}

  explicit Error(const char* fmt) : fmt_(fmt) {}
  ~Error() override = default;

  template <typename... Args>
  Error format(Args&&... args) const {
    Error err = Error(fmt_);
    err.msg_ = Formatter::format(fmt_, std::forward<Args>(args)...);
    return err;
  }

  const char* what() const throw() override { return msg_.c_str(); }

  void print() { std::cout << what() << std::endl; }
};

}  // namespace hshm

// #endif

#endif  // HSHM_ERROR_H
