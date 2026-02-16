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

#ifndef WRP_CTE_BALANCED_MAPPER_H
#define WRP_CTE_BALANCED_MAPPER_H

#include <vector>

#include "abstract_mapper.h"

namespace wrp::cae {
/**
 * Implement balanced mapping
 */
class BalancedMapper : public AbstractMapper {
 public:
  /** Virtual destructor */
  virtual ~BalancedMapper() = default;

  /** Divides an I/O size evenly by into units of page_size */
  void map(size_t off, size_t size, size_t page_size,
           BlobPlacements &ps) override {
    size_t kPageSize = page_size;
    size_t size_mapped = 0;
    while (size > size_mapped) {
      BlobPlacement p;
      p.bucket_off_ = off + size_mapped;
      p.page_ = p.bucket_off_ / kPageSize;
      p.blob_off_ = p.bucket_off_ % kPageSize;
      auto left_size_page = kPageSize - p.blob_off_;
      p.blob_size_ = left_size_page < size - size_mapped ? left_size_page
                                                         : size - size_mapped;
      ps.emplace_back(p);
      size_mapped += p.blob_size_;
    }
  }
};
}  // namespace wrp::cae

#endif  // WRP_CTE_BALANCED_MAPPER_H
