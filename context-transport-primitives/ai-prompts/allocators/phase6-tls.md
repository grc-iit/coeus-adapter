@CLAUDE.md

Create this allocator and implement unit tests for it. the unit tests should include
multi-threaded cases. It should be comparable to malloc in terms of functionality and
generality.

This allocator is intended to be invoked by CPU only.
It will make use of HSHM_THREAD_MODEL->SetTls and GetTls a lot.
We will make a GPU-specific allocator later.

# Class / struct Overview for MultiProcessAllocator

```
class ThreadBlock : slist_node {
    int tid_;
    BuddyAllocator alloc_;  // Private memory is OK here

    ThreadBlock(MemoryBackend backend, size_t size, int tid) {
        // Shift memory backend by (char*)this + sizeof(ThreadBlock) - backend.data_.
        // Set backend size to be size
        // Call shm_init for thread_ with this backend.
    }

    OffsetPtr<char> Allocate(const MemContext &mctx, size_t size) {
        return thread_.AllocateOffset(mctx, size);
    }

    void Expand(OffsetPtr ptr) {
        alloc_.FreeOffset(ptr);
    }
}

class ProcessBlock : slist_node {
    int pid_;
    int tid_count_;
    hshm::Mutex lock_;
    BuddyAllocator alloc_;  // Private memory is OK here
    pre::slist<ThreadBlock> thread_;

    ProcessBlock(const MemoryBackend &backend, void *region) {
        // Call alloc_.shm_init with region
    }

    FullPtr<ThreadBlock> AllocateThreadBlock(const MemoryBackend &backend, size_t region_size) {
        // Acquire lock_
        // Allocate region_size + sizeof(ThreadBlock) from root_
        // If that fails, return null
        // Use tid_count_++ as tid for the ThreadBlock.
        // Cast the region to ThreadBlock* and emplace into slist
        // Call SetTls<ThreadBlock*> and set to this pointer.
    }

    void Expand(OffsetPtr ptr) {
        alloc_.FreeOffset(ptr);
    }
}

class MultiProcessAllocatorHeader {
    int pid_count_;
    pre::slist<ProcessBlock> alloc_procs_;
    pre::slist<ProcessBlock> free_procs_;
    hshm::Mutex lock_;
    BuddyAllocatorHeader alloc_;  // MUST be shared memory
}

class MultiProcessAllocator {
    BuddyAllocator alloc_;

    FullPtr<ProcessBlock> AllocateProcessBlock(const MemoryBackend &backend, size_t region_size) {
        // Acquire lock_ from MultiProcessAllocatorHeader
        // Check if there are any procs in the free_procs_ slist. If so, return that.
        // Allocate region_size + sizeof(ProcessBlock)
        // If that fails, return null
        // Use pid_count_++ as tid for the ThreadBlock.
        // Cast the region to ProcessBlock* and emplace into alloc_procs_
        // Call SetTls<ProcessBlock*> and set to this pointer.
    }

    void FreeProcessBlock() {

    }
}
```

# MultiProcessAllocator

## shm_init

Implementation:
1. Create the MultiProcessAllocatorHeader.
2. Initialize MultiProcessAllocatorHeader.alloc_ with the remainder of the MemoryBackend. 
3. Allocate and construct the first ProcesBlocks from the root_ allocator
4. Emplace into the blocks_ slist.
5. Allocate 

Return Value:
MemContext containing tid and pid of this process.

## shm_attach

Parameters:
1. process_unit_: Unit of process memory allocation. 1GB by default. If we run out of memory for the process,
it will allocate one large chunk of this unit size.
2. thread_unit_: Unit of thread allocation. 16MB by default. If we run out of space for the thread, it will allocate
one large chunk from the process allocator.

implementation:
Call AllocateProcessBlock to allocate a new process block. 

## shm_detach

For now do nothing.

## EnsureTls

1. Check if GetTls<ThreadBlock*> is valid. 
2. If not:
  1. HSHM_THREAD_MODEL->GetTls<ProcessBlock*>
  2. ProcessBlock->AllocateThreadBlock and call GetTls<ThreadBlock*> again.
  3. If it still fails, call MultiProcessAllocator.alloc_ to expand the Process allocator by process_unit_.
  4. Repeat (2). If it still fails, return nullptr.

## AllocateOffset

1. EnsureTLS
2. Call the ThreadBlock* allocator for the size. If that succeeds, return.
3. Acquire ProcessBlock* lock. Allocate max(size, thread_unit_) and expand the thread allocator. retry the thread allocator. Return if not null.
4. Acquire MultiProcessAllocator lock. Allocate max(size, process_unit_) and expand process allocator. Repeat (6). 
5. If still failing, return null.

## ReallocateOffset

1. EnsureTLS
2. Call Reallocate using the ThreadBlock* alloc_. If successful, return.
3. Call AllocateOffset. If null, return null.
4. Copy from old pointer to new pointer. return.

## FreeOffsetNoNullCheck

1. GetTls<ThreadBlock*>. If invalid, return.
2. Call free from alloc_.Free


@CLAUDE.md 

Build a multi-process unit test for the mp allocator.

# Unit Tests

Make a multi-process unit test.
Create a single test file. 
The test takes as input rank, time, nthreads.
The test should allocate, memset, free in a loop for a period of time.

Create a bash script.
Call the test with rank 0, 0 time, and 1 thread to initialize the shared memory.
Call the test with rank 1, 5 time, and 2 threads to attach to the shared memory. Start in background.
Call the test with rank 2, 5 time, and 2 threads to attach to the shared memory. Start in background.
Wait for both tests to complete. Fail if either run into an issue.