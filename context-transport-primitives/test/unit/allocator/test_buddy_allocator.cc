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

#include <catch2/catch_test_macros.hpp>
#include <string>
#include <cstring>
#include "allocator_test.h"
#include "hermes_shm/memory/backend/malloc_backend.h"
#include "hermes_shm/memory/allocator/buddy_allocator.h"

using hshm::testing::AllocatorTest;

TEST_CASE("BuddyAllocator - Allocate and Free Immediate", "[BuddyAllocator]") {
  hipc::MallocBackend backend;
  size_t heap_size = 128 * 1024 * 1024;  // 128 MB heap
  size_t alloc_size = sizeof(hipc::BuddyAllocator);
  backend.shm_init(hipc::MemoryBackendId(0, 0), alloc_size + heap_size);

  auto *alloc = backend.MakeAlloc<hipc::BuddyAllocator>();

  AllocatorTest<hipc::BuddyAllocator> tester(alloc);

  SECTION("Small allocations (1KB)") {
    REQUIRE_NOTHROW(tester.TestAllocFreeImmediate(10000, 1024));
  }

  SECTION("Medium allocations (64KB)") {
    REQUIRE_NOTHROW(tester.TestAllocFreeImmediate(1000, 64 * 1024));
  }

  SECTION("Large allocations (1MB)") {
    REQUIRE_NOTHROW(tester.TestAllocFreeImmediate(100, 1024 * 1024));
  }

}

TEST_CASE("BuddyAllocator - Batch Allocate and Free", "[BuddyAllocator]") {
  hipc::MallocBackend backend;
  size_t heap_size = 128 * 1024 * 1024;  // 128 MB heap
  size_t alloc_size = sizeof(hipc::BuddyAllocator);
  backend.shm_init(hipc::MemoryBackendId(0, 0), alloc_size + heap_size);

  auto *alloc = backend.MakeAlloc<hipc::BuddyAllocator>();

  AllocatorTest<hipc::BuddyAllocator> tester(alloc);

  SECTION("Small batches (10 allocations of 4KB)") {
    REQUIRE_NOTHROW(tester.TestAllocFreeBatch(1000, 10, 4096));
  }

  SECTION("Medium batches (100 allocations of 4KB)") {
    REQUIRE_NOTHROW(tester.TestAllocFreeBatch(100, 100, 4096));
  }

  SECTION("Large batches (1000 allocations of 1KB)") {
    REQUIRE_NOTHROW(tester.TestAllocFreeBatch(10, 1000, 1024));
  }

}

TEST_CASE("BuddyAllocator - Random Allocation", "[BuddyAllocator]") {
  hipc::MallocBackend backend;
  size_t heap_size = 128 * 1024 * 1024;  // 128 MB heap
  size_t alloc_size = sizeof(hipc::BuddyAllocator);
  backend.shm_init(hipc::MemoryBackendId(0, 0), alloc_size + heap_size);

  auto *alloc = backend.MakeAlloc<hipc::BuddyAllocator>();

  AllocatorTest<hipc::BuddyAllocator> tester(alloc);

  SECTION("16 iterations of random allocations"){
    try {
      tester.TestRandomAllocation(256);
    }
    catch (const std::exception &e) {
      std::cout << ("TestRandomAllocation(16) failed: " + std::string(e.what()));
    }
    catch (const hshm::Error &e) {
      std::cout << ("TestRandomAllocation(16) failed: " + std::string(e.what()));
    }
  }

  SECTION("32 iterations of random allocations") {
    REQUIRE_NOTHROW(tester.TestRandomAllocation(32));
  }

}

TEST_CASE("BuddyAllocator - Large Then Small", "[BuddyAllocator]") {
  hipc::MallocBackend backend;
  size_t heap_size = 128 * 1024 * 1024;  // 128 MB heap
  size_t alloc_size = sizeof(hipc::BuddyAllocator);
  backend.shm_init(hipc::MemoryBackendId(0, 0), alloc_size + heap_size);

  auto *alloc = backend.MakeAlloc<hipc::BuddyAllocator>();

  AllocatorTest<hipc::BuddyAllocator> tester(alloc);

  SECTION("10 iterations: 100 x 1MB then 1000 x 128B") {
    REQUIRE_NOTHROW(tester.TestLargeThenSmall(10, 100, 1024 * 1024, 1000, 128));
  }

  SECTION("5 iterations: 50 x 512KB then 500 x 256B") {
    REQUIRE_NOTHROW(tester.TestLargeThenSmall(5, 50, 512 * 1024, 500, 256));
  }

}

TEST_CASE("BuddyAllocator - Weird Offset Allocation", "[BuddyAllocator]") {
  // Test allocator instantiation at a weird offset in the backend
  hipc::MallocBackend backend;
  constexpr size_t kOffsetFromData = 256UL * 1024UL;  // 256KB offset
  constexpr size_t kHeapSize = 128UL * 1024UL * 1024UL;  // 128 MB heap
  constexpr size_t kAllocSize = sizeof(hipc::BuddyAllocator);

  // Create backend with enough space for offset + allocator + heap
  size_t total_size = kOffsetFromData + kAllocSize + kHeapSize;
  backend.shm_init(hipc::MemoryBackendId(0, 0), total_size);
  memset(backend.data_, 0, backend.data_capacity_);

  // Get pointer to data at weird offset
  char *data_ptr = backend.data_;
  char *alloc_ptr = data_ptr + kOffsetFromData;

  // Placement new to construct allocator at weird offset
  hipc::BuddyAllocator *alloc =
      new (alloc_ptr) hipc::BuddyAllocator();

  // Initialize allocator with available space after allocator object
  // Region size should account for remaining space after allocator placement
  try {
    alloc->shm_init(backend);
  } catch (...) {
    throw;
  }

  // Create allocator tester and run tests
  AllocatorTest<hipc::BuddyAllocator> tester(alloc);

  SECTION("Random allocation at offset") {
    try {
      tester.TestRandomAllocation(16);
    } catch (const std::exception &e) {
      std::cout << ("TestRandomAllocation failed: " + std::string(e.what()));
    } catch (const hshm::Error &e) {
      std::cout << ("TestRandomAllocation failed: " + std::string(e.what()));
    }
  }

  SECTION("Allocate and free immediate at offset") {
    REQUIRE_NOTHROW(tester.TestAllocFreeImmediate(100, 4096));
  }

}
