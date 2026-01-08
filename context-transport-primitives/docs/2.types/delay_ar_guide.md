# delay_ar: Delayed Archive for Shared Memory Objects

## Overview

The `delay_ar<T>` class (located in `include/hermes_shm/data_structures/internal/shm_archive.h`) is a fundamental building block in HSHM that provides delayed initialization for objects in shared memory. It acts as a wrapper that delays the construction of an object until it's explicitly initialized, which is crucial for shared memory data structures that need careful initialization timing.

## Key Features

- **Delayed Initialization**: Objects are not constructed until `shm_init()` is called
- **Type-Aware Allocation**: Automatically handles both SHM-aware and primitive types
- **Memory Layout Control**: Provides exact control over object placement in shared memory
- **Serialization Support**: Integrated with serialization frameworks like Cereal
- **Zero-Overhead Access**: Direct pointer access with minimal overhead

## Template Parameters

- `T`: The type of object to be stored and managed

## Core Methods

### Initialization

```cpp
// Initialize with no arguments (for default constructible types)
void shm_init();

// Initialize with arguments
template<typename... Args>
void shm_init(Args&&... args);

// Initialize with allocator (for SHM-aware types)
template<typename AllocT, typename... Args>  
void shm_init(AllocT&& alloc, Args&&... args);

// Initialize piecewise (advanced usage)
template<typename ArgPackT_1, typename ArgPackT_2>
void shm_init_piecewise(ArgPackT_1&& args1, ArgPackT_2&& args2);
```

### Access

```cpp
T* get();                    // Get pointer to object
const T* get() const;        // Get const pointer
T& get_ref();               // Get reference to object  
const T& get_ref() const;   // Get const reference
T& operator*();             // Dereference operator
T* operator->();            // Arrow operator
```

### Destruction

```cpp
void shm_destroy();  // Explicitly destroy the contained object
```

## Usage Examples

### Basic Primitive Types

```cpp
#include "hermes_shm/data_structures/internal/shm_archive.h"

// Integer storage
hipc::delay_ar<int> int_archive;
int_archive.shm_init(42);
std::cout << *int_archive << std::endl;  // Prints: 42

// String storage  
hipc::delay_ar<std::string> string_archive;
string_archive.shm_init("Hello, World!");
std::cout << string_archive->c_str() << std::endl;  // Prints: Hello, World!
```

### SHM-Aware Container Types

```cpp
#include "hermes_shm/data_structures/ipc/vector.h"

// Vector with allocator
auto alloc = HSHM_MEMORY_MANAGER->GetDefaultAllocator<hipc::StackAllocator>();
hipc::delay_ar<hipc::vector<int>> vec_archive;

// For SHM-aware types, pass allocator first
vec_archive.shm_init(alloc, 100);  // Create vector with capacity 100
vec_archive->push_back(1);
vec_archive->push_back(2);
vec_archive->push_back(3);

std::cout << "Vector size: " << vec_archive->size() << std::endl;
```

### Custom Object Initialization

```cpp
struct CustomObject {
    int value;
    std::string name;
    
    CustomObject(int v, const std::string& n) : value(v), name(n) {}
};

hipc::delay_ar<CustomObject> custom_archive;
custom_archive.shm_init(100, "MyObject");

std::cout << custom_archive->name << ": " << custom_archive->value << std::endl;
```

### Pair Example (Common Pattern)

```cpp
#include "hermes_shm/data_structures/ipc/pair.h"

// Pair of integers
hipc::delay_ar<hipc::pair<int, std::string>> pair_archive;
auto alloc = GetSomeAllocator();
pair_archive.shm_init(alloc);

// Access pair elements
pair_archive->first_.shm_init(alloc, 42);
pair_archive->second_.shm_init(alloc, "Hello");

std::cout << "First: " << *pair_archive->first_ << std::endl;
std::cout << "Second: " << *pair_archive->second_ << std::endl;
```

### Array of Objects

```cpp
// Array of delay_ar objects
constexpr size_t ARRAY_SIZE = 10;
hipc::delay_ar<int> int_array[ARRAY_SIZE];

// Initialize each element
for (size_t i = 0; i < ARRAY_SIZE; ++i) {
    int_array[i].shm_init(static_cast<int>(i * 10));
}

// Access elements
for (size_t i = 0; i < ARRAY_SIZE; ++i) {
    std::cout << "Element " << i << ": " << *int_array[i] << std::endl;
}
```

### Cleanup Example

```cpp
hipc::delay_ar<std::vector<int>> vector_archive;
vector_archive.shm_init();
vector_archive->push_back(1);

// Explicit cleanup when done
vector_archive.shm_destroy();
```

## Type Behavior Differences

### SHM-Aware Types (inherit from ShmContainer)

SHM-aware types like `hipc::vector`, `hshm::priv::string`, `hipc::pair` require an allocator:

```cpp
auto alloc = GetAllocator();
hipc::delay_ar<hipc::vector<int>> shm_vec;
shm_vec.shm_init(alloc, initial_capacity);  // Allocator passed as first argument
```

### Non-SHM-Aware Types (primitives, STL types)

Regular types don't need allocators:

```cpp
hipc::delay_ar<std::vector<int>> std_vec;
std_vec.shm_init(initial_capacity);  // No allocator needed
```

## Advanced Usage: Piecewise Construction

For complex initialization scenarios:

```cpp
hipc::delay_ar<std::pair<std::string, int>> complex_pair;
complex_pair.shm_init_piecewise(
    make_argpack("First string"),    // Arguments for first element
    make_argpack(42)                 // Arguments for second element
);
```

## Serialization Support

The `delay_ar` class integrates with serialization frameworks:

```cpp
#include <cereal/archives/binary.hpp>

hipc::delay_ar<hipc::vector<int>> vec_archive;
vec_archive.shm_init(alloc, 10);
vec_archive->push_back(1);
vec_archive->push_back(2);

// Serialize
std::ostringstream os;
cereal::BinaryOutputArchive output_archive(os);
output_archive << vec_archive;

// Deserialize  
hipc::delay_ar<hipc::vector<int>> restored_archive;
std::istringstream is(os.str());
cereal::BinaryInputArchive input_archive(is);
input_archive >> restored_archive;  // Automatically calls shm_init
```

## Best Practices

1. **Always Initialize**: Never access a `delay_ar` object without calling `shm_init()` first
2. **Match Constructor Signatures**: Ensure arguments to `shm_init()` match the target type's constructor
3. **Allocator Consistency**: For SHM-aware types, use the same allocator throughout related objects
4. **Explicit Cleanup**: Call `shm_destroy()` when manual cleanup is needed
5. **Exception Safety**: Be aware that initialization can throw exceptions

## Common Patterns in HSHM Codebase

### Container Member Variables

```cpp
class MyDataStructure : public hipc::ShmContainer {
private:
    hipc::delay_ar<hipc::vector<int>> data_;
    hipc::delay_ar<hshm::priv::string> name_;

public:
    void shm_init(hipc::Allocator* alloc, const std::string& name) {
        init_shm_container(alloc);
        data_.shm_init(GetCtxAllocator(), 100);
        name_.shm_init(GetCtxAllocator(), name);
    }
};
```

### Array Initialization in Allocators

```cpp
class PageAllocator {
    hipc::delay_ar<LifoListQueue> free_lists_[NUM_CACHES];

public:
    void Initialize(hipc::Allocator* alloc) {
        for (size_t i = 0; i < NUM_CACHES; ++i) {
            free_lists_[i].shm_init(alloc);
        }
    }
};
```
