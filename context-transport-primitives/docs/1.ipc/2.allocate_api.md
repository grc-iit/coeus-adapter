# Hermes Shared Memory Allocation API Documentation

## Overview

The Hermes Shared Memory (HSHM) allocation API provides a sophisticated memory management system designed for high-performance shared memory applications. The API supports multiple allocator types, type-safe memory operations, and seamless integration between process-local and shared memory pointers.

## Core Concepts

### FullPtr<T, PointerT>

The `FullPtr` is the fundamental abstraction that encapsulates both process-local and shared memory pointers:

```cpp
template<typename T, typename PointerT = Pointer>
struct FullPtr {
    T* ptr_;           // Process-local pointer (fast access)
    PointerT shm_;     // Shared memory pointer (serializable)
};
```

**Key Features:**
- **Dual representation**: Contains both fast process-local pointer and serializable shared memory offset
- **Type safety**: Template-based type checking at compile time
- **Conversion support**: Easy casting between different types
- **Null checking**: Built-in null pointer detection

### Memory Context

Memory operations can benefit from a context that provides thread-local information. :

```cpp
class MemContext {
public:
    ThreadId tid_ = ThreadId::GetNull();  // Thread identifier for thread-local allocators
};
```

For the default context, we have a macro:
```cpp
HSHM_MCTX
```

This should be fine for the vast majority of allocations.

## Allocator Types

### 1. StackAllocator
- **Use case**: Simple linear allocation, no deallocation
- **Performance**: Fastest allocation (O(1))
- **Limitation**: No individual deallocation support

### 2. MallocAllocator  
- **Use case**: General-purpose allocation using system malloc
- **Performance**: Standard system allocation performance
- **Features**: Full malloc/free semantics

### 3. ScalablePageAllocator
- **Use case**: High-performance page-based allocation
- **Performance**: Fast allocation with good fragmentation control
- **Features**: Thread-safe, supports reallocation

### 4. ThreadLocalAllocator
- **Use case**: Thread-local caching for reduced contention
- **Performance**: Excellent multi-threaded performance
- **Features**: Per-thread memory pools, automatic thread management

## Core API Functions

### Allocation Functions

#### Basic Allocation

```cpp
template <typename T = void, typename PointerT = Pointer>
FullPtr<T, PointerT> Allocate(const MemContext &ctx, size_t size);
```

**Example:**
```cpp
// Allocate 1024 bytes
auto full_ptr = alloc->template Allocate<void>(HSHM_DEFAULT_MEM_CTX, 1024);
void* ptr = full_ptr.ptr_;         // Process-local pointer
Pointer shm_ptr = full_ptr.shm_;   // Shared memory pointer

// Type-specific allocation
auto int_ptr = alloc->template Allocate<int>(HSHM_DEFAULT_MEM_CTX, sizeof(int) * 100);
int* ints = int_ptr.ptr_;          // Direct access to int array
```

#### Aligned Allocation

```cpp
template <typename T = void, typename PointerT = Pointer>
FullPtr<T, PointerT> AlignedAllocate(const MemContext &ctx, size_t size, size_t alignment);
```

**Example:**
```cpp
// Allocate 4KB page-aligned memory
auto aligned_ptr = alloc->template AlignedAllocate<char>(
    HSHM_DEFAULT_MEM_CTX, 4096, 4096);
char* page = aligned_ptr.ptr_;
assert(((uintptr_t)page % 4096) == 0);  // Verify alignment
```

#### Reallocation

```cpp
template <typename T = void, typename PointerT = Pointer>
FullPtr<T, PointerT> Reallocate(const MemContext &ctx, 
                                FullPtr<T, PointerT> &old_ptr, 
                                size_t new_size);
```

**Example:**
```cpp
// Initial allocation
auto data = alloc->template Allocate<char>(HSHM_DEFAULT_MEM_CTX, 1024);
strcpy(data.ptr_, "Hello, World!");

// Expand to larger size
auto expanded = alloc->template Reallocate<char>(HSHM_DEFAULT_MEM_CTX, data, 2048);
// Original data is preserved
assert(strcmp(expanded.ptr_, "Hello, World!") == 0);
```

#### Deallocation

```cpp
template <typename T = void, typename PointerT = Pointer>
void Free(const MemContext &ctx, FullPtr<T, PointerT> &ptr);
```

**Example:**
```cpp
auto memory = alloc->template Allocate<int>(HSHM_DEFAULT_MEM_CTX, 1000 * sizeof(int));
// Use the memory...
alloc->template Free<int>(HSHM_DEFAULT_MEM_CTX, memory);
```

## Object-Oriented API

### Single Object Operations

#### Object Construction
```cpp
template<typename T, typename ...Args>
FullPtr<T> NewObj(const MemContext &ctx, Args&&... args);
```

**Example:**
```cpp
// Create a std::vector with initial capacity
auto vec_ptr = alloc->NewObj<std::vector<int>>(HSHM_DEFAULT_MEM_CTX, 100);
vec_ptr.ptr_->push_back(42);
vec_ptr.ptr_->push_back(24);
```

#### Object Destruction
```cpp
template<typename T>
void DelObj(const MemContext &ctx, FullPtr<T> &ptr);
```

**Example:**
```cpp
auto obj = alloc->NewObj<std::string>(HSHM_DEFAULT_MEM_CTX, "Hello HSHM!");
// Use the object...
alloc->DelObj(HSHM_DEFAULT_MEM_CTX, obj);  // Calls destructor and frees memory
```

### Array Object Operations

#### Array Construction
```cpp
template<typename T>
FullPtr<T> NewObjs(const MemContext &ctx, size_t count);
```

**Example:**
```cpp
// Create array of 50 integers
auto int_array = alloc->NewObjs<int>(HSHM_DEFAULT_MEM_CTX, 50);
for (int i = 0; i < 50; ++i) {
    int_array.ptr_[i] = i * 2;
}
```

#### Array Reallocation
```cpp
template<typename T>
FullPtr<T> ReallocateObjs(const MemContext &ctx, FullPtr<T> &ptr, size_t new_count);
```

**Example:**
```cpp
auto objects = alloc->NewObjs<std::string>(HSHM_DEFAULT_MEM_CTX, 10);
// Initialize strings...
for (int i = 0; i < 10; ++i) {
    new (objects.ptr_ + i) std::string("Item " + std::to_string(i));
}

// Expand array to 20 elements
objects = alloc->ReallocateObjs<std::string>(HSHM_DEFAULT_MEM_CTX, objects, 20);
```

#### Array Destruction
```cpp
template<typename T>
void DelObjs(const MemContext &ctx, FullPtr<T> &ptr, size_t count);
```

## Advanced Usage Patterns

### Working with Custom Types

```cpp
struct CustomData {
    int id;
    char name[32];
    
    CustomData(int i, const char* n) : id(i) {
        strncpy(name, n, 31);
        name[31] = '\0';
    }
};

// Allocate and construct custom object
auto custom = alloc->NewObj<CustomData>(HSHM_DEFAULT_MEM_CTX, 42, "MyObject");
printf("Created object: id=%d, name=%s\n", custom.ptr_->id, custom.ptr_->name);
alloc->DelObj(HSHM_DEFAULT_MEM_CTX, custom);
```

### Pointer Conversion and Management

```cpp
// Allocate FullPtr
FullPtr<void> orig_ptr = alloc->Allocate(HSHM_MCTX, 1024);

// Create FullPtr from private pointer 
// This will automatically determine the shared pointer
FullPtr<void> full_ptr(orig_ptr.ptr_);

// Create FullPtr from shared pointer
// This will automatically determine the private pionter.
FullPtr<void> full_ptr2(orig_ptr.shm_);

// Type casting
FullPtr<int> int_ptr = full_ptr.Cast<int>();
FullPtr<char> char_ptr = full_ptr.Cast<char>();

// Null checking
if (!full_ptr.IsNull()) {
    // Safe to use pointer
    memset(full_ptr.ptr_, 0, 1024);
}
```

### Multi-threaded Usage

```cpp
// Thread-local allocator example
void worker_thread(ThreadLocalAllocator* alloc, int thread_id) {
    MemContext ctx(ThreadId(thread_id));
    
    // Each thread gets its own memory pool
    auto local_data = alloc->template Allocate<WorkerData>(ctx, sizeof(WorkerData));
    
    // Perform thread-local work...
    process_data(local_data.ptr_);
    
    // Cleanup
    alloc->template Free<WorkerData>(ctx, local_data);
}
```

## Memory Backend Integration

### Allocator Setup

```cpp
#include "hermes_shm/memory/memory_manager.h"

// Initialize memory backend
auto mem_manager = HSHM_MEMORY_MANAGER;
mem_manager->CreateBackendWithUrl<hipc::PosixShmMmap>(
    hipc::MemoryBackendId::Get(0), 
    hshm::Unit<size_t>::Gigabytes(1), 
    "my_shared_memory"
);

// Create allocator
AllocatorId alloc_id(1, 0);
mem_manager->CreateAllocator<hipc::ScalablePageAllocator>(
    hipc::MemoryBackendId::Get(0), 
    alloc_id, 
    0  // No custom header
);

// Get allocator instance
auto alloc = mem_manager->GetAllocator<hipc::ScalablePageAllocator>(alloc_id);
```

### Custom Headers

```cpp
struct MyAllocatorHeader {
    uint64_t magic_number;
    size_t allocation_count;
};

// Create allocator with custom header
mem_manager->CreateAllocator<hipc::ScalablePageAllocator>(
    hipc::MemoryBackendId::Get(0), 
    alloc_id, 
    sizeof(MyAllocatorHeader)
);

// Access custom header
auto header = alloc->template GetCustomHeader<MyAllocatorHeader>();
header->magic_number = 0xDEADBEEF;
header->allocation_count = 0;
```

## Performance Considerations

### Choosing the Right Allocator

1. **StackAllocator**: Best for temporary, short-lived allocations
2. **ThreadLocalAllocator**: Optimal for multi-threaded applications with frequent allocations
3. **ScalablePageAllocator**: Good general-purpose choice with reallocation support
4. **MallocAllocator**: Use when system malloc behavior is required

### Best Practices

1. **Minimize Allocations**: Batch allocate when possible
2. **Use Appropriate Types**: Specify template parameters for better type safety
3. **Thread Context**: Always provide appropriate MemContext for thread-local allocators
4. **Memory Tracking**: Enable `HSHM_ALLOC_TRACK_SIZE` for debugging memory leaks

### Memory Leak Detection

```cpp
// Check for memory leaks
size_t before = alloc->GetCurrentlyAllocatedSize();
{
    auto temp = alloc->template Allocate<char>(HSHM_DEFAULT_MEM_CTX, 1024);
    // Use memory...
    alloc->template Free<char>(HSHM_DEFAULT_MEM_CTX, temp);
}
size_t after = alloc->GetCurrentlyAllocatedSize();
assert(before == after);  // No memory leak
```

## Error Handling

The allocator API throws exceptions for error conditions:

```cpp
try {
    // This may throw if out of memory
    auto huge_alloc = alloc->template Allocate<char>(HSHM_DEFAULT_MEM_CTX, SIZE_MAX);
} catch (const hshm::Error& e) {
    if (e.code() == hshm::ErrorCode::OUT_OF_MEMORY) {
        printf("Out of memory: requested=%zu, available=%zu\n", 
               SIZE_MAX, alloc->GetCurrentlyAllocatedSize());
    }
}
```

## Migration Guide

When migrating from older versions:

1. **Template Parameters**: Add explicit template parameters to allocation functions
2. **FullPtr Usage**: Replace separate pointer and shared memory offset handling with FullPtr
3. **Memory Context**: Ensure proper MemContext is passed to all operations
4. **Type Safety**: Leverage template parameters for compile-time type checking

## Examples Summary

The API provides a comprehensive memory management solution with:
- Type-safe allocation and deallocation
- Support for both raw memory and constructed objects
- Multiple allocator implementations for different use cases
- Seamless integration with shared memory systems
- Thread-safe operations with minimal contention
- Built-in memory leak detection and debugging support