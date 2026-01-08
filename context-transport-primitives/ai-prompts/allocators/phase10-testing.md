# Unit Testing Allocators

I want there to be a single workload generator used by ALL allocators. This is what context-transport-primitives/test/unit/allocator/allocator_test.h is for.

If there are specific workloads meant to stress certain allocators, please add them to this unified allocator test!!!!

Do NOT create custom workloads outside of this file. EVERY SINGLE ALLOCATOR SHOULD HAVE ACCESS TO THE SAME WORKLOADS!!!!!!! IT SHOULD BE UNIFORM!!!!

When you CREATE ALLOCATORS. ALWAYS, USE THE MakeAlloc or AttachAlloc methods of the backend!!!! Stop manually casting the backned and then using new and shm_init manually!!!!!! IT'S BAD PRACTICE. 

