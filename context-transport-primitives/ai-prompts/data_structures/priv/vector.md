@CLAUDE.md

Add a todo list. 

# Data structure unit tests.

Let's split context-transport-primitives/include/hermes_shm/data_structures in to two subdirectories.
Move the contents of everythign currently there under ipc.
Create a new directory called priv for the new data structures we will be creating.

# Vector

Implement a private-memory vector and iterators for it in context-transport-primitives/include/hermes_shm/data_structures/priv/vector.h.
It should implement similar methods to std::vector along with similar iterators.
Handle piece-of-data (POD) types differently from classes. 
POD types should support using memcpy and memset for initialization.
Implement the various types of constructors, operators, and methods based on:
https://en.cppreference.com/w/cpp/container/vector.html
https://en.cppreference.com/w/cpp/container/vector/vector.html

It should support GPU and CPU

AllocT should be stored as a pointer instead of a copy