# HSHM Allocator and Backend Management Guide

## Overview

The HSHM Memory Management system provides a two-tier architecture where Memory Backends manage raw memory resources and Allocators manage allocation strategies within those backends. This guide demonstrates how to create, destroy, and attach allocators and backends for both single-process and distributed (MPI) applications.

## Core Concepts

### Memory Backends

Memory backends provide the underlying storage mechanism:

```cpp
#include "hermes_shm/memory/memory_manager.h"

namespace hshm::ipc {
    enum class MemoryBackendType {
        kPosixShmMmap,      // POSIX shared memory with mmap
        kMallocBackend,     // Standard malloc/free
        kArrayBackend,      // Pre-allocated array
        kPosixMmap,         // POSIX mmap files
        kGpuMalloc,         // GPU malloc (CUDA/ROCm)
        kGpuShmMmap,        // GPU shared memory
    };
}
```

### Allocators

Allocators implement different allocation strategies:

- **StackAllocator**: Linear allocation (fast, no fragmentation)
- **ScalablePageAllocator**: Page-based allocation (balanced performance/flexibility)
- **ThreadLocalAllocator**: Thread-local allocation pools
- **MallocAllocator**: Wrapper around standard malloc

## Single-Process Allocator Management

### Basic Backend and Allocator Setup

```cpp
#include "hermes_shm/memory/memory_manager.h"
#include "hermes_shm/memory/allocator/stack_allocator.h"
#include "hermes_shm/memory/backend/posix_shm_mmap.h"

void basic_allocator_setup_example() {
    // Get the memory manager singleton
    auto* mem_mngr = HSHM_MEMORY_MANAGER;
    
    // Define IDs
    hshm::ipc::MemoryBackendId backend_id = hipc::MemoryBackendId::Get(0);
    hshm::ipc::AllocatorId alloc_id(1, 0);  // major=1, minor=0
    
    // Step 1: Create a memory backend
    std::string shm_url = "my_shared_memory";
    size_t backend_size = hshm::Unit<size_t>::Gigabytes(1);
    
    mem_mngr->CreateBackend<hipc::PosixShmMmap>(
        backend_id, backend_size, shm_url);
    
    printf("Created backend with ID %u, size %zu GB\n", 
           backend_id.id_, backend_size / (1024*1024*1024));
    
    // Step 2: Create an allocator on the backend
    size_t custom_header_size = sizeof(int);  // Space for custom metadata
    mem_mngr->CreateAllocator<hipc::StackAllocator>(
        backend_id, alloc_id, custom_header_size);
    
    // Step 3: Get the allocator for use
    auto* allocator = mem_mngr->GetAllocator<hipc::StackAllocator>(alloc_id);
    if (allocator) {
        printf("✓ Stack allocator created successfully\n");
        
        // Initialize custom header
        auto* custom_header = allocator->template GetCustomHeader<int>();
        *custom_header = 0x12345678;  // Custom metadata
        
        // Use the allocator
        auto full_ptr = allocator->Allocate(HSHM_DEFAULT_MEM_CTX, 1024);
        printf("Allocated %zu bytes at offset %zu\n", 
               size_t(1024), full_ptr.shm_.off_.load());
        
        // Free the memory
        allocator->Free(HSHM_DEFAULT_MEM_CTX, full_ptr);
        
        // Verify custom header persisted
        printf("Custom header: 0x%08X\n", *custom_header);
    }
    
    // Step 4: Cleanup (usually done at program exit)
    mem_mngr->UnregisterAllocator(alloc_id);
    mem_mngr->DestroyBackend(backend_id);
    
    printf("Cleanup completed\n");
}
```

### Multiple Allocators on Single Backend
Not supported.

## Distributed (MPI) Allocator Management

### MPI Rank 0 (Leader) Setup

```cpp
#include <mpi.h>

template<typename AllocatorT>
void SetupMpiAllocatorRank0() {
    // This runs only on MPI rank 0 (leader process)
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    if (rank != 0) return;
    
    printf("Rank 0: Setting up shared memory backend and allocator\\n");
    
    auto* mem_mngr = HSHM_MEMORY_MANAGER;
    std::string shm_url = "mpi_shared_allocator";
    hshm::ipc::MemoryBackendId backend_id = hipc::MemoryBackendId::Get(0);
    hshm::ipc::AllocatorId alloc_id(1, 0);
    
    // Clean up any existing resources
    mem_mngr->UnregisterAllocator(alloc_id);
    mem_mngr->DestroyBackend(backend_id);
    
    // Create shared memory backend
    mem_mngr->CreateBackend<hipc::PosixShmMmap>(
        backend_id, hshm::Unit<size_t>::Gigabytes(1), shm_url);
    
    // Create allocator with custom header space
    struct AllocatorHeader {
        int magic_number;
        size_t total_allocations;
        size_t peak_usage;
    };
    
    mem_mngr->CreateAllocator<AllocatorT>(
        backend_id, alloc_id, sizeof(AllocatorHeader));
    
    auto* allocator = mem_mngr->GetAllocator<AllocatorT>(alloc_id);
    if (allocator) {
        // Initialize shared header
        auto* header = allocator->template GetCustomHeader<AllocatorHeader>();
        header->magic_number = 0xDEADBEEF;
        header->total_allocations = 0;
        header->peak_usage = 0;
        
        printf("Rank 0: ✓ Created %s with shared header\\n", 
               typeid(AllocatorT).name());
    }
}
```

### MPI Non-Leader Ranks Setup

```cpp
template<typename AllocatorT>
void SetupMpiAllocatorRankN() {
    // This runs on all MPI ranks except 0 (follower processes)
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    if (rank == 0) return;
    
    printf("Rank %d: Attaching to shared memory backend\\n", rank);
    
    auto* mem_mngr = HSHM_MEMORY_MANAGER;
    std::string shm_url = "mpi_shared_allocator";
    hshm::ipc::MemoryBackendId backend_id = hipc::MemoryBackendId::Get(0);
    hshm::ipc::AllocatorId alloc_id(1, 0);
    
    // Clean up any existing local state
    mem_mngr->UnregisterAllocator(alloc_id);
    mem_mngr->DestroyBackend(backend_id);
    
    // Attach to existing shared memory (created by rank 0)
    mem_mngr->AttachBackend(hshm::ipc::MemoryBackendType::kPosixShmMmap, shm_url);
    
    // Get the allocator (already created by rank 0)
    auto* allocator = mem_mngr->GetAllocator<AllocatorT>(alloc_id);
    if (allocator) {
        struct AllocatorHeader {
            int magic_number;
            size_t total_allocations;
            size_t peak_usage;
        };
        
        // Verify we can access shared header
        auto* header = allocator->template GetCustomHeader<AllocatorHeader>();
        if (header->magic_number == 0xDEADBEEF) {
            printf("Rank %d: ✓ Successfully attached to shared allocator\\n", rank);
        } else {
            printf("Rank %d: ✗ Shared header validation failed\\n", rank);
        }
    } else {
        printf("Rank %d: ✗ Failed to get allocator after attach\\n", rank);
    }
}
```

## Best Practices

1. **Backend Selection**:
   - Use `PosixShmMmap` for multi-process applications requiring shared memory
   - Use `MallocBackend` for single-process applications requiring maximum speed
   - Use `PosixMmap` for persistent storage requirements
   - Use `ArrayBackend` for embedded systems or when memory is pre-allocated

2. **MPI Setup Pattern**:
   - Rank 0 creates backends and allocators
   - Other ranks attach to existing shared memory
   - Use barriers to synchronize setup phases
   - Clean up existing resources before creating new ones

3. **Resource Management**:
   - Always unregister allocators before destroying backends
   - Use RAII patterns when possible
   - Check allocation success before using pointers
   - Monitor memory usage through allocator statistics

4. **Custom Headers**:
   - Use atomic types for multi-process shared statistics
   - Include version numbers for compatibility checking
   - Add application-specific metadata as needed
   - Initialize headers immediately after allocator creation

5. **Error Handling**:
   - Check return values from all allocation operations
   - Handle backend creation failures gracefully
   - Verify shared memory attachment in MPI scenarios
   - Implement retry logic for transient failures

6. **Performance Optimization**:
   - Choose appropriate allocator types for workload patterns
   - Monitor fragmentation and allocation/deallocation patterns
   - Use aligned allocations when needed for SIMD operations
   - Consider thread-local allocators for high-frequency allocations

7. **Testing and Validation**:
   - Test with various allocation sizes and patterns
   - Verify data integrity after allocations
   - Test alignment requirements for specialized workloads
   - Validate proper cleanup and resource deallocation