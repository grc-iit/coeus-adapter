/*
 * Copyright (c) 2024, Gnosis Research Center, Illinois Institute of Technology
 * All rights reserved.
 *
 * This file is part of IOWarp Core.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WRP_CTE_ADAPTER_ADAPTER_TYPES_H_
#define WRP_CTE_ADAPTER_ADAPTER_TYPES_H_

#include "adapter/posix/posix_api.h"

namespace wrp::cae {

/** Adapter types */
enum class AdapterType {
  kNone,
  kPosix,
  kStdio,
  kMpiio,
  kPubsub,
  kVfd
};

/** Adapter modes */
enum class AdapterMode {
  kNone,
  kDefault,
  kBypass,
  kScratch,
  kWorkflow
};

/**
 * Per-Object Adapter Settings.
 * An object may be a file, for example.
 * */
struct AdapterObjectConfig {
  AdapterMode mode_;
  size_t page_size_;
};

/** Adapter Mode converter */
class AdapterModeConv {
 public:
  static std::string str(AdapterMode mode) {
    switch (mode) {
      case AdapterMode::kDefault: {
        return "AdapterMode::kDefault";
      }
      case AdapterMode::kBypass: {
        return "AdapterMode::kBypass";
      }
      case AdapterMode::kScratch: {
        return "AdapterMode::kScratch";
      }
      case AdapterMode::kWorkflow: {
        return "AdapterMode::kWorkflow";
      }
      default: {
        return "Unkown adapter mode";
      }
    }
  }

  static AdapterMode to_enum(const std::string &mode) {
    if (mode.find("kDefault") != std::string::npos) {
      return AdapterMode::kDefault;
    } else if (mode.find("kBypass") != std::string::npos) {
      return AdapterMode::kBypass;
    } else if (mode.find("kScratch") != std::string::npos) {
      return AdapterMode::kScratch;
    } else if (mode.find("kWorkflow") != std::string::npos) {
      return AdapterMode::kWorkflow;
    }
    return AdapterMode::kDefault;
  }
};

struct AdapterInfo {
  int file_id_;
  int fd_;
  int open_flags_;
  int mode_flags_;
  int refcnt_;
  std::string path_;
  AdapterMode adapter_mode_;

  ~AdapterInfo() {
    if (fd_ >= 0) {
      WRP_CTE_POSIX_API->close(fd_);
    }
  }
};

}  // namespace wrp::cae

#endif  // WRP_CTE_ADAPTER_ADAPTER_TYPES_H_
