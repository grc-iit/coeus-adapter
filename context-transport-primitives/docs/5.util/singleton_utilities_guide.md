# HSHM Singleton Utilities Guide

## Overview

The Singleton Utilities API in Hermes Shared Memory (HSHM) provides multiple singleton patterns optimized for different use cases, including thread safety, cross-device compatibility, and performance requirements. These utilities enable global state management across complex applications and shared memory systems.

## Singleton Variants

### Basic Singleton (Thread-Safe)

```cpp
#include "hermes_shm/util/singleton.h"

class DatabaseConfig {
public:
    std::string connection_string;
    int max_connections;
    
    DatabaseConfig() {
        connection_string = "localhost:5432";
        max_connections = 100;
    }
    
    void Configure(const std::string& host, int max_conn) {
        connection_string = host;
        max_connections = max_conn;
    }
};

// Thread-safe singleton access
DatabaseConfig* config = hshm::Singleton<DatabaseConfig>::GetInstance();
config->Configure("prod-db:5432", 200);

// Multiple access from different threads
void worker_thread() {
    DatabaseConfig* cfg = hshm::Singleton<DatabaseConfig>::GetInstance();
    printf("Connecting to: %s\n", cfg->connection_string.c_str());
}
```

### Lockfree Singleton (High Performance)

```cpp
class MetricsCollector {
    std::atomic<size_t> counter_;
    
public:
    MetricsCollector() : counter_(0) {}
    
    void Increment() {
        counter_.fetch_add(1, std::memory_order_relaxed);
    }
    
    size_t GetCount() const {
        return counter_.load(std::memory_order_relaxed);
    }
};

// High-performance singleton without locking overhead
void hot_path_function() {
    auto* metrics = hshm::LockfreeSingleton<MetricsCollector>::GetInstance();
    metrics->Increment();  // Very fast, no locks
}
```

### Cross-Device Singleton

```cpp
class GPUManager {
public:
    int device_count;
    std::vector<int> available_devices;
    
    GPUManager() {
        device_count = GetGPUCount();
        InitializeDevices();
    }
    
private:
    int GetGPUCount();
    void InitializeDevices();
};

// Works on both host and GPU code
HSHM_CROSS_FUN
void initialize_cuda_context() {
    GPUManager* gpu_mgr = hshm::CrossSingleton<GPUManager>::GetInstance();
    printf("Found %d GPU devices\n", gpu_mgr->device_count);
}

// Lockfree version for GPU performance
HSHM_CROSS_FUN
void gpu_kernel_function() {
    auto* gpu_mgr = hshm::LockfreeCrossSingleton<GPUManager>::GetInstance();
    // Access without locking overhead in GPU kernels
}
```

### Global Singleton (Eager Initialization)

```cpp
class Logger {
public:
    std::ofstream log_file;
    std::mutex log_mutex;
    
    Logger() {
        log_file.open("/var/log/application.log", std::ios::app);
        printf("Logger initialized during program startup\n");
    }
    
    void Log(const std::string& message) {
        std::lock_guard<std::mutex> lock(log_mutex);
        log_file << "[" << GetTimestamp() << "] " << message << std::endl;
    }
    
private:
    std::string GetTimestamp();
};

// Initialized immediately when program starts
Logger* logger = hshm::GlobalSingleton<Logger>::GetInstance();

void application_function() {
    // Logger already exists and is ready
    hshm::GlobalSingleton<Logger>::GetInstance()->Log("Function called");
}
```

### Platform-Aware Global Singleton

```cpp
class NetworkManager {
public:
    std::string local_hostname;
    std::vector<std::string> network_interfaces;
    
    NetworkManager() {
        DiscoverNetworkInterfaces();
        printf("Network manager initialized\n");
    }
    
private:
    void DiscoverNetworkInterfaces();
};

// Automatically chooses best implementation for platform
HSHM_CROSS_FUN
void network_operation() {
    auto* net_mgr = hshm::GlobalCrossSingleton<NetworkManager>::GetInstance();
    printf("Local hostname: %s\n", net_mgr->local_hostname.c_str());
}
```

## C-Style Global Variable Singletons

### Basic Global Variables

```cpp
// Header declaration
HSHM_DEFINE_GLOBAL_VAR_H(DatabaseConfig, g_db_config);

// Source file definition  
HSHM_DEFINE_GLOBAL_VAR_CC(DatabaseConfig, g_db_config);

// Usage
void configure_database() {
    DatabaseConfig* config = HSHM_GET_GLOBAL_VAR(DatabaseConfig, g_db_config);
    config->Configure("prod:5432", 500);
}
```

### Cross-Platform Global Variables

```cpp
class SharedMemoryPool {
public:
    size_t pool_size;
    void* memory_base;
    
    SharedMemoryPool() : pool_size(0), memory_base(nullptr) {
        InitializePool();
    }
    
private:
    void InitializePool();
};

// Header - works on host and device
HSHM_DEFINE_GLOBAL_CROSS_VAR_H(SharedMemoryPool, g_memory_pool);

// Source file
HSHM_DEFINE_GLOBAL_CROSS_VAR_CC(SharedMemoryPool, g_memory_pool);

// Usage in cross-platform code
HSHM_CROSS_FUN
void allocate_from_pool(size_t size) {
    SharedMemoryPool* pool = HSHM_GET_GLOBAL_CROSS_VAR(SharedMemoryPool, g_memory_pool);
    // Allocation logic here
}
```

### Pointer-Based Global Variables

```cpp
class TaskScheduler {
public:
    std::queue<std::function<void()>> task_queue;
    std::mutex queue_mutex;
    std::condition_variable queue_cv;
    bool running;
    
    TaskScheduler() : running(true) {
        printf("Task scheduler created\n");
    }
    
    void SubmitTask(std::function<void()> task);
    void ProcessTasks();
    void Shutdown();
};

// Header - pointer version for lazy initialization
HSHM_DEFINE_GLOBAL_PTR_VAR_H(TaskScheduler, g_task_scheduler);

// Source file
HSHM_DEFINE_GLOBAL_PTR_VAR_CC(TaskScheduler, g_task_scheduler);

// Usage - automatically creates instance on first access
void submit_work() {
    TaskScheduler* scheduler = HSHM_GET_GLOBAL_PTR_VAR(TaskScheduler, g_task_scheduler);
    
    scheduler->SubmitTask([]() {
        printf("Task executing\n");
    });
}
```

### Cross-Platform Pointer Variables

```cpp
class DeviceMemoryManager {
public:
    size_t total_memory;
    size_t available_memory;
    std::map<void*, size_t> allocations;
    
    DeviceMemoryManager() {
        QueryDeviceMemory();
    }
    
private:
    void QueryDeviceMemory();
};

// Header
HSHM_DEFINE_GLOBAL_CROSS_PTR_VAR_H(DeviceMemoryManager, g_device_memory);

// Source file  
HSHM_DEFINE_GLOBAL_CROSS_PTR_VAR_CC(DeviceMemoryManager, g_device_memory);

// Cross-platform usage
HSHM_CROSS_FUN
void* allocate_device_memory(size_t size) {
    DeviceMemoryManager* mgr = HSHM_GET_GLOBAL_CROSS_PTR_VAR(DeviceMemoryManager, g_device_memory);
    // Device-specific allocation
    return nullptr; // Implementation specific
}
```

## Macro Wrappers for Global Variable Singletons

### Simplifying Access with Macros

For frequently used singletons, create convenient macro wrappers to reduce code verbosity and provide cleaner API access:

```cpp
// Define convenient macros for common singletons
#define DATABASE_CONFIG hshm::Singleton<DatabaseConfig>::GetInstance()
#define METRICS_COLLECTOR hshm::LockfreeSingleton<MetricsCollector>::GetInstance()
#define GPU_MANAGER hshm::CrossSingleton<GPUManager>::GetInstance()
#define LOGGER hshm::GlobalSingleton<Logger>::GetInstance()
#define NETWORK_MANAGER hshm::GlobalCrossSingleton<NetworkManager>::GetInstance()

// Global variable style macros
#define MEMORY_POOL HSHM_GET_GLOBAL_VAR(SharedMemoryPool, g_memory_pool)
#define TASK_SCHEDULER HSHM_GET_GLOBAL_PTR_VAR(TaskScheduler, g_task_scheduler)
#define DEVICE_MEMORY HSHM_GET_GLOBAL_CROSS_PTR_VAR(DeviceMemoryManager, g_device_memory)
```

### Usage Examples with Macros

**Before** - Verbose singleton access:
```cpp
void configure_system() {
    // Verbose and repetitive
    hshm::Singleton<DatabaseConfig>::GetInstance()->Configure("prod:5432", 500);
    hshm::LockfreeSingleton<MetricsCollector>::GetInstance()->Increment();
    hshm::GlobalSingleton<Logger>::GetInstance()->Log("System configured");
    
    // Long variable declarations
    auto* gpu_mgr = hshm::CrossSingleton<GPUManager>::GetInstance();
    auto* net_mgr = hshm::GlobalCrossSingleton<NetworkManager>::GetInstance();
}
```

**After** - Clean macro access:
```cpp
void configure_system() {
    // Clean and concise
    DATABASE_CONFIG->Configure("prod:5432", 500);
    METRICS_COLLECTOR->Increment();
    LOGGER->Log("System configured");
    
    // Short, readable access
    GPU_MANAGER->device_count;
    NETWORK_MANAGER->local_hostname;
}
```

### Recommended Macro Naming Conventions

```cpp
// 1. SCREAMING_SNAKE_CASE for singleton instances
#define CONFIG_MANAGER hshm::Singleton<ConfigManager>::GetInstance()
#define CACHE_MANAGER hshm::LockfreeSingleton<CacheManager>::GetInstance()

// 2. Prefix with component name for large applications
#define DB_CONNECTION_POOL hshm::Singleton<ConnectionPool>::GetInstance()
#define DB_QUERY_CACHE hshm::LockfreeSingleton<QueryCache>::GetInstance()

// 3. Use descriptive names that match functionality
#define THREAD_POOL hshm::GlobalSingleton<ThreadPoolManager>::GetInstance()
#define ERROR_REPORTER hshm::CrossSingleton<ErrorReporter>::GetInstance()

// 4. For global variables, match the variable name pattern
#define SHARED_BUFFER HSHM_GET_GLOBAL_VAR(SharedBuffer, g_shared_buffer)
#define TEMP_ALLOCATOR HSHM_GET_GLOBAL_PTR_VAR(TempAllocator, g_temp_alloc)
```

### Advanced Macro Patterns

**Conditional Access Macros:**
```cpp
// Macro with null check for optional singletons
#define SAFE_LOGGER (LOGGER ? LOGGER : &null_logger_instance)

// Debug-only singleton access
#ifdef DEBUG
#define DEBUG_PROFILER hshm::Singleton<Profiler>::GetInstance()
#else
#define DEBUG_PROFILER (&null_profiler_instance)
#endif
```

**Functional Macros:**
```cpp
// Macro that performs common operations
#define LOG_INFO(msg) LOGGER->Log(LogLevel::INFO, msg)
#define LOG_ERROR(msg) LOGGER->Log(LogLevel::ERROR, msg)
#define INCREMENT_COUNTER(name) METRICS_COLLECTOR->IncrementCounter(name)
#define RECORD_LATENCY(name, duration) METRICS_COLLECTOR->RecordLatency(name, duration)
```

**Type-Safe Wrapper Macros:**
```cpp
// Wrapper with type checking
#define GET_CONFIG(type) \
    (static_cast<type*>(hshm::Singleton<ConfigRegistry>::GetInstance()->Get(#type)))

// Usage: auto* db_cfg = GET_CONFIG(DatabaseConfig);
```

### Best Practices for Singleton Macros

1. **Consistency**: Use the same naming convention across your entire codebase
2. **Documentation**: Document what each macro expands to and its thread safety guarantees
3. **Scope**: Place macro definitions in a central header file included by all modules
4. **Namespace**: Consider using a prefix to avoid naming conflicts
5. **Type Safety**: Ensure macros maintain type safety and don't hide important type information
6. **Debugging**: Make macros debugger-friendly - avoid complex expressions
7. **Performance**: Use appropriate singleton type (lockfree vs thread-safe) based on usage patterns

### Header File Organization

```cpp
// singletons.h - Central singleton definitions
#ifndef PROJECT_SINGLETONS_H
#define PROJECT_SINGLETONS_H

#include "hermes_shm/util/singleton.h"
#include "config/database_config.h"
#include "metrics/metrics_collector.h"
#include "logging/logger.h"

// Define all singleton access macros
#define DATABASE_CONFIG hshm::Singleton<DatabaseConfig>::GetInstance()
#define METRICS_COLLECTOR hshm::LockfreeSingleton<MetricsCollector>::GetInstance()
#define LOGGER hshm::GlobalSingleton<Logger>::GetInstance()

// Functional convenience macros
#define LOG_INFO(msg) LOGGER->Info(msg)
#define LOG_ERROR(msg) LOGGER->Error(msg)
#define COUNT(metric) METRICS_COLLECTOR->Increment(metric)

#endif // PROJECT_SINGLETONS_H
```

## Best Practices

1. **Thread Safety**: Use `Singleton<T>` for thread-safe access, `LockfreeSingleton<T>` only with thread-safe types
2. **Cross-Platform Code**: Use `CrossSingleton<T>` and `GlobalCrossSingleton<T>` for code that runs on both host and device
3. **Python Compatibility**: Avoid standard singletons in code called by Python; use global variables instead
4. **Eager vs Lazy**: Use `GlobalSingleton<T>` for resources needed at startup, regular singletons for lazy initialization
5. **Resource Management**: Implement proper destructors and cleanup in singleton classes
6. **Configuration**: Use singletons for application-wide configuration and settings
7. **Performance**: Use lockfree variants in performance-critical paths with appropriate atomic types
8. **Memory Management**: Be aware that singletons live for the entire program duration
9. **Testing**: Design singleton classes to be testable by allowing dependency injection where possible
10. **Documentation**: Document singleton lifetime and thread safety guarantees for each singleton class
11. **Macro Wrappers**: Create convenient macro wrappers for frequently accessed singletons to improve code readability
12. **Naming Conventions**: Use consistent SCREAMING_SNAKE_CASE naming for singleton access macros
