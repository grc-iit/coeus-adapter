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

#include "basic_test.h"
#include "hermes_shm/thread/lock.h"
#include "omp.h"

using hshm::Mutex;
using hshm::RwLock;

void MutexTest(int nthreads) {
  size_t loop_count = 10000;
  size_t count = 0;
  Mutex lock;

  omp_set_dynamic(0);
#pragma omp parallel shared(lock) num_threads(nthreads)
  {
    // Support parallel write
#pragma omp barrier
    for (size_t i = 0; i < loop_count; ++i) {
      lock.Lock(i);
      count += 1;
      lock.Unlock();
    }
#pragma omp barrier
    REQUIRE(count == loop_count * nthreads);
#pragma omp barrier
  }
}

void RwLockTest(int producers, int consumers, size_t loop_count) {
  size_t nthreads = producers + consumers;
  size_t count = 0;
  RwLock lock;

  omp_set_dynamic(0);
#pragma omp parallel shared(lock, nthreads, producers, consumers, loop_count, \
                                count) num_threads(nthreads)
  {  // NOLINT
    int tid = omp_get_thread_num();

#pragma omp barrier
    size_t total_size = producers * loop_count;
    if (tid < consumers) {
      // The left 2 threads will be readers
      lock.ReadLock(tid);
      for (size_t i = 0; i < loop_count; ++i) {
        REQUIRE(count <= total_size);
      }
      lock.ReadUnlock();
    } else {
      // The right 4 threads will be writers
      lock.WriteLock(tid);
      for (size_t i = 0; i < loop_count; ++i) {
        count += 1;
      }
      lock.WriteUnlock();
    }

#pragma omp barrier
    REQUIRE(count == total_size);
  }
}

TEST_CASE("Mutex") { MutexTest(8); }

TEST_CASE("RwLock") {
  RwLockTest(8, 0, 1000000);
  RwLockTest(7, 1, 1000000);
  RwLockTest(4, 4, 1000000);
}

#if HSHM_ENABLE_THALLIUM
TEST_CASE("AbtThread") {
  hshm::thread::Argobots argobots;
  hshm::thread::ThreadGroup group = argobots.CreateThreadGroup({});
  hshm::thread::Thread thread = argobots.Spawn(
      group,
      [](int tid) { std::cout << "Hello, world! (abt) " << tid << std::endl; },
      1);
  argobots.Join(thread);
}
#endif

#ifdef HSHM_ENABLE_PTHREADS
TEST_CASE("Pthread") {
  hshm::thread::Pthread pthread;
  hshm::thread::ThreadGroup group = pthread.CreateThreadGroup({});
  hshm::thread::Thread thread = pthread.Spawn(
      group,
      [](int tid) {
        std::cout << "Hello, world! (pthread) " << tid << std::endl;
      },
      1);
  pthread.Join(thread);
}
#endif

TEST_CASE("StdThread") {
  hshm::thread::StdThread std_thread;
  hshm::thread::ThreadGroup group = std_thread.CreateThreadGroup({});
  hshm::thread::Thread thread = std_thread.Spawn(
      group,
      [](int tid) { std::cout << "Hello, world! (std) " << tid << std::endl; },
      1);
  std_thread.Join(thread);
}
