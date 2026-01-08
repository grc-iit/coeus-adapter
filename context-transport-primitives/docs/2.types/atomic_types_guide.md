# HSHM Atomic Types Guide

## Overview

The Atomic Types API in Hermes Shared Memory (HSHM) provides cross-platform atomic operations with support for CPU, GPU (CUDA/ROCm), and non-atomic variants. The API abstracts platform differences and provides consistent atomic operations for thread-safe programming across different execution environments.

## Atomic Type Variants

### Platform-Specific Atomic Types

```cpp
#include "hermes_shm/types/atomic.h"

void atomic_variants_example() {
    // Standard atomic (uses std::atomic on host, GPU atomics on device)
    hshm::ipc::atomic<int> standard_atomic(42);
    
    // Non-atomic (for single-threaded or externally synchronized code)
    hshm::ipc::nonatomic<int> non_atomic_value(100);
    
    // Explicit GPU atomic (CUDA/ROCm specific)
#if HSHM_ENABLE_CUDA || HSHM_ENABLE_ROCM
    hshm::ipc::rocm_atomic<int> gpu_atomic(200);
#endif
    
    // Explicit standard library atomic
    hshm::ipc::std_atomic<int> std_lib_atomic(300);
    
    // Conditional atomic - chooses atomic or non-atomic based on template parameter
    hshm::ipc::opt_atomic<int, true>  conditional_atomic(400);     // Uses atomic
    hshm::ipc::opt_atomic<int, false> conditional_nonatomic(500); // Uses nonatomic
    
    printf("Standard atomic: %d\n", standard_atomic.load());
    printf("Non-atomic: %d\n", non_atomic_value.load());
    printf("Conditional atomic: %d\n", conditional_atomic.load());
}
```

## Basic Atomic Operations

### Load, Store, and Exchange

```cpp
void basic_atomic_operations() {
    hshm::ipc::atomic<int> counter(0);
    
    // Load value
    int current = counter.load();
    printf("Current value: %d\n", current);
    
    // Store new value
    counter.store(10);
    printf("After store(10): %d\n", counter.load());
    
    // Exchange (atomically set new value and return old)
    int old_value = counter.exchange(20);
    printf("Exchange returned: %d, new value: %d\n", old_value, counter.load());
    
    // Compare and exchange (conditional atomic update)
    int expected = 20;
    bool success = counter.compare_exchange_weak(expected, 30);
    printf("CAS success: %s, value: %d\n", success ? "yes" : "no", counter.load());
    
    // Try CAS with wrong expected value
    expected = 25;  // Wrong expected value
    success = counter.compare_exchange_strong(expected, 40);
    printf("CAS with wrong expected: %s, value: %d, expected now: %d\n", 
           success ? "yes" : "no", counter.load(), expected);
}
```

### Arithmetic Operations

```cpp
void arithmetic_operations_example() {
    hshm::ipc::atomic<int> counter(10);
    
    // Fetch and add
    int old_val = counter.fetch_add(5);
    printf("fetch_add(5): old=%d, new=%d\n", old_val, counter.load());
    
    // Fetch and subtract
    old_val = counter.fetch_sub(3);
    printf("fetch_sub(3): old=%d, new=%d\n", old_val, counter.load());
    
    // Increment operators
    ++counter;  // Pre-increment
    printf("After pre-increment: %d\n", counter.load());
    
    counter++;  // Post-increment
    printf("After post-increment: %d\n", counter.load());
    
    // Decrement operators
    --counter;  // Pre-decrement
    printf("After pre-decrement: %d\n", counter.load());
    
    counter--;  // Post-decrement
    printf("After post-decrement: %d\n", counter.load());
    
    // Assignment operators
    counter += 10;
    printf("After += 10: %d\n", counter.load());
    
    counter -= 5;
    printf("After -= 5: %d\n", counter.load());
}
```

### Bitwise Operations

```cpp
void bitwise_operations_example() {
    hshm::ipc::atomic<uint32_t> flags(0xF0F0F0F0);
    
    printf("Initial flags: 0x%08X\n", flags.load());
    
    // Bitwise AND
    uint32_t result = (flags & 0xFF00FF00).load();
    printf("flags & 0xFF00FF00 = 0x%08X\n", result);
    
    // Bitwise OR
    result = (flags | 0x0F0F0F0F).load();
    printf("flags | 0x0F0F0F0F = 0x%08X\n", result);
    
    // Bitwise XOR
    result = (flags ^ 0xFFFFFFFF).load();
    printf("flags ^ 0xFFFFFFFF = 0x%08X\n", result);
    
    // Assignment bitwise operations
    flags &= 0xFF00FF00;
    printf("After &= 0xFF00FF00: 0x%08X\n", flags.load());
    
    flags |= 0x0F0F0F0F;
    printf("After |= 0x0F0F0F0F: 0x%08X\n", flags.load());
    
    flags ^= 0x12345678;
    printf("After ^= 0x12345678: 0x%08X\n", flags.load());
}
```

## Conditional Atomic Types

```cpp
template<bool THREAD_SAFE>
class ConfigurableCounter {
    hshm::ipc::opt_atomic<int, THREAD_SAFE> count_;
    
public:
    ConfigurableCounter() : count_(0) {}
    
    void Increment() {
        count_.fetch_add(1);
    }
    
    void Add(int value) {
        count_.fetch_add(value);
    }
    
    int Get() const {
        return count_.load();
    }
    
    void Reset() {
        count_.store(0);
    }
};

void conditional_atomic_example() {
    // Thread-safe version
    ConfigurableCounter<true> thread_safe_counter;
    
    // Non-atomic version for single-threaded use
    ConfigurableCounter<false> fast_counter;
    
    const int iterations = 100000;
    
    // Test thread-safe version with multiple threads
    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([&thread_safe_counter, iterations]() {
            for (int j = 0; j < iterations; ++j) {
                thread_safe_counter.Increment();
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    // Test non-atomic version (single-threaded)
    for (int i = 0; i < 4 * iterations; ++i) {
        fast_counter.Increment();
    }
    
    printf("Thread-safe counter: %d\n", thread_safe_counter.Get());
    printf("Fast counter: %d\n", fast_counter.Get());
    printf("Both should equal: %d\n", 4 * iterations);
}
```

## Serialization Support

```cpp
#include <sstream>
#include <cereal/archives/binary.hpp>

void atomic_serialization_example() {
    hshm::ipc::atomic<int> counter(12345);
    hshm::ipc::nonatomic<double> value(3.14159);
    
    // Serialize to binary stream
    std::stringstream ss;
    {
        cereal::BinaryOutputArchive archive(ss);
        archive(counter, value);
    }
    
    // Deserialize from binary stream
    hshm::ipc::atomic<int> loaded_counter;
    hshm::ipc::nonatomic<double> loaded_value;
    {
        cereal::BinaryInputArchive archive(ss);
        archive(loaded_counter, loaded_value);
    }
    
    printf("Original counter: %d, loaded: %d\n", 
           counter.load(), loaded_counter.load());
    printf("Original value: %f, loaded: %f\n", 
           value.load(), loaded_value.load());
}
```

## Best Practices

1. **Platform Selection**: Use `hshm::ipc::atomic<T>` for automatic platform selection (CPU vs GPU)
2. **Performance**: Use `nonatomic<T>` for single-threaded code or when external synchronization is provided
3. **Memory Ordering**: Specify appropriate memory ordering for performance-critical code
4. **GPU Compatibility**: Use HSHM atomic types for code that runs on both CPU and GPU
5. **Lock-Free Design**: Prefer atomic operations over locks for high-performance concurrent code
6. **Reference Counting**: Use atomic counters for thread-safe reference counting implementations
7. **Conditional Compilation**: Use `opt_atomic<T, bool>` for compile-time atomic vs non-atomic selection
8. **Cross-Platform**: All atomic types work consistently across different architectures and GPUs
9. **Serialization**: Atomic types support standard serialization for persistence and communication
10. **Testing**: Always test atomic code under high contention to verify correctness and performance