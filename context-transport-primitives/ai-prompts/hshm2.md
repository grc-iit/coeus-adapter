Consolidate include/hermes_shm/memory/allocator/allocator.h to include only apis that return a FullPtr<T>, where T is default void. E.g., NewObj, NewObjs, etc. should now return FullPtr. 

All allocators in this directory should return FullPtr<void> instead of hipc::ShmPtr<>.

Ensure that all uses of the changed or deleted functions are modified accordingly.

Remove all APIs for Array and LArray in /mnt/home/Projects/iowarp/cte-hermes-shm/include/hermes_shm/memory/memory.h. Ensure that all unit tests relying on this are removed.