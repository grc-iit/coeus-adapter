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

#ifndef CHIMAERA_INCLUDE_CHIMAERA_CORWLOCK_H_
#define CHIMAERA_INCLUDE_CHIMAERA_CORWLOCK_H_

#include <hermes_shm/thread/lock/rwlock.h>

namespace chi {

/**
 * CoRwLock - Simple wrapper around hshm::RwLock
 */
using CoRwLock = hshm::RwLock;

/**
 * ScopedCoRwReadLock - RAII read lock wrapper with default owner
 * Wraps hshm::ScopedRwReadLock with a default owner value of 0
 */
struct ScopedCoRwReadLock {
  hshm::ScopedRwReadLock scoped_lock_;

  /** Acquire the read lock with default owner */
  explicit ScopedCoRwReadLock(CoRwLock& lock)
      : scoped_lock_(lock, 0) {}

  /** Release handled by scoped_lock_ destructor */
  ~ScopedCoRwReadLock() = default;
};

/**
 * ScopedCoRwWriteLock - RAII write lock wrapper with default owner
 * Wraps hshm::ScopedRwWriteLock with a default owner value of 0
 */
struct ScopedCoRwWriteLock {
  hshm::ScopedRwWriteLock scoped_lock_;

  /** Acquire the write lock with default owner */
  explicit ScopedCoRwWriteLock(CoRwLock& lock)
      : scoped_lock_(lock, 0) {}

  /** Release handled by scoped_lock_ destructor */
  ~ScopedCoRwWriteLock() = default;
};

}  // namespace chi

#endif  // CHIMAERA_INCLUDE_CHIMAERA_CORWLOCK_H_
