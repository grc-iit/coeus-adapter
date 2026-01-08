# HSHM Timer Utilities Guide

## Overview

The Timer Utilities API in Hermes Shared Memory (HSHM) provides high-resolution timing capabilities for performance measurement, profiling, and benchmarking. The API includes basic timers, MPI-aware distributed timing, thread-local timing, and periodic execution utilities.

## Core Timer Classes

### Basic High-Resolution Timer

```cpp
#include "hermes_shm/util/timer.h"

void basic_timing_example() {
    // Create a high-resolution timer
    hshm::Timer timer;
    
    // Start timing
    timer.Reset();  // Starts timer and resets accumulated time
    
    // Simulate some work
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Pause and get elapsed time
    timer.Pause();  // Adds elapsed time to accumulated total
    
    // Access timing results
    double elapsed_ns = timer.GetNsec();      // Nanoseconds
    double elapsed_us = timer.GetUsec();      // Microseconds  
    double elapsed_ms = timer.GetMsec();      // Milliseconds
    double elapsed_s = timer.GetSec();        // Seconds
    
    printf("Operation took %.2f milliseconds\n", elapsed_ms);
    
    // Resume timing for additional work
    timer.Resume();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    timer.Pause();
    
    printf("Total time: %.2f milliseconds\n", timer.GetMsec());
}
```

### Timer Types Available

```cpp
// Different timer implementations
hshm::HighResCpuTimer cpu_timer;           // std::chrono::high_resolution_clock
hshm::HighResMonotonicTimer mono_timer;    // std::chrono::steady_clock (recommended)
hshm::Timer default_timer;                 // Alias for HighResMonotonicTimer

// Timepoint classes for manual timing
hshm::HighResCpuTimepoint cpu_timepoint;
hshm::HighResMonotonicTimepoint mono_timepoint;
hshm::Timepoint default_timepoint;         // Alias for HighResMonotonicTimepoint

void timepoint_example() {
    hshm::Timepoint start, end;
    
    start.Now();  // Capture current time
    
    // Do work
    expensive_computation();
    
    end.Now();
    double elapsed_ms = end.GetMsecFromStart(start);
    printf("Computation took %.2f milliseconds\n", elapsed_ms);
}
```

## MPI Distributed Timing

```cpp
#include "hermes_shm/util/timer_mpi.h"

#if HSHM_ENABLE_MPI
void mpi_timing_example(MPI_Comm comm) {
    hshm::MpiTimer mpi_timer(comm);
    
    // Each rank performs timing
    mpi_timer.Reset();
    
    // Simulate different work on each rank
    int rank;
    MPI_Comm_rank(comm, &rank);
    std::this_thread::sleep_for(std::chrono::milliseconds(50 + rank * 10));
    
    mpi_timer.Pause();
    
    // Collect timing statistics across all ranks
    
    // Get maximum time across all ranks
    mpi_timer.CollectMax();
    if (rank == 0) {
        printf("Max time across all ranks: %.2f ms\n", mpi_timer.GetMsec());
    }
    
    // Get minimum time across all ranks  
    mpi_timer.Reset();
    std::this_thread::sleep_for(std::chrono::milliseconds(50 + rank * 10));
    mpi_timer.Pause();
    mpi_timer.CollectMin();
    if (rank == 0) {
        printf("Min time across all ranks: %.2f ms\n", mpi_timer.GetMsec());
    }
    
    // Get average time across all ranks (default)
    mpi_timer.Reset();
    std::this_thread::sleep_for(std::chrono::milliseconds(50 + rank * 10));
    mpi_timer.Pause();
    mpi_timer.Collect();  // Same as CollectAvg()
    if (rank == 0) {
        printf("Average time across all ranks: %.2f ms\n", mpi_timer.GetMsec());
    }
}
#endif
```

## Thread-Local Timing

```cpp
#include "hermes_shm/util/timer_thread.h"

class WorkerPool {
    std::vector<std::thread> workers_;
    hshm::ThreadTimer thread_timer_;
    
public:
    explicit WorkerPool(int num_threads) : thread_timer_(num_threads) {
        for (int i = 0; i < num_threads; ++i) {
            workers_.emplace_back([this, i]() {
                WorkerThread(i);
            });
        }
    }
    
    void Join() {
        for (auto& worker : workers_) {
            worker.join();
        }
        
        // Collect timing from all threads
        thread_timer_.Collect();
        printf("Max thread time: %.2f ms\n", thread_timer_.GetMsec());
    }
    
private:
    void WorkerThread(int thread_id) {
        // Set thread rank for timing
        thread_timer_.SetRank(thread_id);
        
        // Perform timed work
        thread_timer_.Reset();
        
        // Simulate different amounts of work per thread
        for (int i = 0; i < 1000 * (thread_id + 1); ++i) {
            // Some computation
            volatile double x = sin(i * 0.001);
            (void)x;  // Prevent optimization
        }
        
        thread_timer_.Pause();
        
        printf("Thread %d completed in %.2f ms\n", 
               thread_id, thread_timer_.timers_[thread_id].GetMsec());
    }
};

void thread_timing_example() {
    const int num_threads = 4;
    WorkerPool pool(num_threads);
    
    pool.Join();
}
```

## Best Practices

1. **Timer Choice**: Use `hshm::Timer` (monotonic) for measuring durations, avoid CPU timers that can be affected by frequency scaling
2. **MPI Timing**: Use `MpiTimer` for measuring distributed operations and getting consistent timing across ranks
3. **Thread Safety**: `ThreadTimer` provides thread-local timing; use `TimerPool` for complex multi-threaded scenarios
4. **Periodic Operations**: Use `HSHM_PERIODIC` macros for regular maintenance tasks without additional timer overhead
5. **Warm-up**: Always perform warm-up runs before benchmarking to account for CPU frequency scaling and cache effects
6. **Statistical Analysis**: Use multiple measurements and calculate statistics for reliable performance characterization
7. **Overhead Awareness**: Be aware of timing overhead (typically 10-100ns) when measuring very short operations
8. **Cross-Platform**: All timers work consistently across different platforms and provide nanosecond precision
9. **Memory Management**: Timers are lightweight but consider pooling for high-frequency timing scenarios
10. **Integration**: Combine with profiling tools and performance monitoring systems for comprehensive analysis