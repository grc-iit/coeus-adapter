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

// Copyright 2025 Chimaera Project
// Licensed under the Apache License, Version 2.0

#ifndef CHIMAERA_INCLUDE_CHIMAERA_INTEGER_TIMER_H_
#define CHIMAERA_INCLUDE_CHIMAERA_INTEGER_TIMER_H_

#include "chimaera/types.h"

namespace chi {

/**
 * Integer-based timepoint for performance optimization.
 * Uses a simple u64 counter instead of real time measurements.
 */
class IntegerTimepoint {
 public:
  u64 microseconds_;  /**< Time in microseconds since start */

  /** Default constructor */
  IntegerTimepoint() : microseconds_(0) {}

  /** Constructor with microseconds value */
  explicit IntegerTimepoint(u64 us) : microseconds_(us) {}

  /** Get microseconds value */
  u64 GetUsec() const { return microseconds_; }

  /** Comparison operators */
  bool operator<(const IntegerTimepoint &other) const {
    return microseconds_ < other.microseconds_;
  }

  bool operator>(const IntegerTimepoint &other) const {
    return microseconds_ > other.microseconds_;
  }

  bool operator<=(const IntegerTimepoint &other) const {
    return microseconds_ <= other.microseconds_;
  }

  bool operator>=(const IntegerTimepoint &other) const {
    return microseconds_ >= other.microseconds_;
  }

  bool operator==(const IntegerTimepoint &other) const {
    return microseconds_ == other.microseconds_;
  }

  bool operator!=(const IntegerTimepoint &other) const {
    return microseconds_ != other.microseconds_;
  }

  /** Addition operator */
  IntegerTimepoint operator+(const IntegerTimepoint &other) const {
    return IntegerTimepoint(microseconds_ + other.microseconds_);
  }

  /** Subtraction operator */
  IntegerTimepoint operator-(const IntegerTimepoint &other) const {
    return IntegerTimepoint(microseconds_ - other.microseconds_);
  }

  /** Addition assignment operator */
  IntegerTimepoint& operator+=(const IntegerTimepoint &other) {
    microseconds_ += other.microseconds_;
    return *this;
  }

  /** Subtraction assignment operator */
  IntegerTimepoint& operator-=(const IntegerTimepoint &other) {
    microseconds_ -= other.microseconds_;
    return *this;
  }
};

/**
 * Integer-based timer for performance optimization.
 * Uses a static counter instead of real time measurements.
 */
class IntegerTimer {
 public:
  static u64 current_time_us_;  /**< Current fake time in microseconds */

  /**
   * Get current timepoint.
   * @return Current IntegerTimepoint
   */
  static IntegerTimepoint Now() {
    return IntegerTimepoint(current_time_us_);
  }

  /**
   * Increment time by 1 microsecond.
   */
  static void Increment() {
    ++current_time_us_;
  }

  /**
   * Reset time to 0.
   */
  static void Reset() {
    current_time_us_ = 0;
  }
};

}  // namespace chi

#endif  // CHIMAERA_INCLUDE_CHIMAERA_INTEGER_TIMER_H_
