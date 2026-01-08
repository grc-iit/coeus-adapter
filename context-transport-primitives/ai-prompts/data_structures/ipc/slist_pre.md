@CLAUDE.md

# Singly-Linked List Preallocated

Create this data structure in context-transport-primitives/include/hermes_shm/data_structures/ipc/slist_pre.h

This data structure does not perform allocations. It assumes the entries are pre-allocated.
This is a shared-memory compatible data structure. 

Build a unit test under context-transport-primitives/test/unit/data_structures for this class.
The unit test can use the ArenaAllocator over a MallocBackend.

## class slist 

```
namespace hshm::ipc::pre {

class slist {
    size_t size_;
    OffsetPointer head_;
};

}
```

## class slist_node

```
namespace hshm::ipc::pre {

class slist_node {
    OffsetPointer next_;
}

}
```

## emplace

Parameters: 
1. Allocator *alloc (the allocator used for the node)
2. FullPtr<SLLNode> node (the node to emplace)

This will emplace at the front of the list.
1. Set "node->next" to head.
2. Set head to node.
3. Increment count.

## pop

Parameters:
1. Allocator *alloc (the allocator used for the node)

Output:
1. FullPtr<slist_node>

This will pop the first entry.
1. Verify size is not 0. Return FullPtr::GetNull if it is
2. auto head = FullPtr<slist_node>(alloc, head_)
3. head_ = head->next_;
4. count--

## size

Return the counter size_;
