# Hermes SHM Logging Guide

This guide covers the HILOG and HELOG logging macros provided by Hermes Shared Memory (HSHM) for structured logging and error reporting.

## Overview

The Hermes SHM logging system provides two main macros for different types of logging:
- `HILOG`: For informational logging
- `HELOG`: For error logging

Both macros are built on top of the underlying `HLOG` macro and provide structured, thread-safe logging with configurable verbosity levels.

## Log Levels

The system defines several predefined log levels:

| Level     | Code | Description                        | Output  |
|-----------|------|------------------------------------|---------|
| `kInfo`   | 251  | Useful information for users       | stdout  |
| `kWarning`| 252  | Something might be wrong           | stderr  |
| `kError`  | 253  | A non-fatal error has occurred     | stderr  |
| `kFatal`  | 254  | A fatal error (causes program exit)| stderr  |
| `kDebug`  | 255/-1| Low-priority debugging info       | stdout  |

## HILOG (Hermes Info Log)

### Syntax
```cpp
HLOG(SUB_CODE, format_string, ...args)
```

### Purpose
Logs informational messages at the `kInfo` level. These messages are displayed on stdout and provide useful information to users about program execution.

### Parameters
- `SUB_CODE`: A sub-category code to further classify the log message
- `format_string`: Printf-style format string
- `...args`: Arguments for the format string

### Output Format
```
filepath:line INFO thread_id function_name message
```

### Examples

#### Basic Information Logging
```cpp
HLOG(kInfo, "Server started on port {}", 8080);
// Output: /path/to/file.cc:45 INFO 12345 main Server started on port 8080
```

#### Performance Metrics
```cpp
HLOG(kInfo, "{},{},{},{},{},{} ms,{} KOps", 
      test_name, alloc_type, obj_size, msec, nthreads, count, kops);
// Output: /path/to/file.cc:170 INFO 12345 benchmark_func test_malloc,malloc,1024,50 ms,4,1000000 KOps
```

#### Debug Logging (Debug Builds Only)
```cpp
HLOG(kDebug, "Acquired read lock for {}", owner);
// Output (debug builds): /path/to/file.cc:108 INFO 12345 acquire_lock Acquired read lock for thread_123
```

#### Status Messages
```cpp
HLOG(kInfo, "Lz4: output buffer is potentially too small");
HLOG(kInfo, "test_name,alloc_type,obj_size,msec,nthreads,count,KOps");
```

## HELOG (Hermes Error Log)

### Syntax
```cpp
HLOG(LOG_CODE, format_string, ...args)
```

### Purpose
Logs error messages using the same code for both the primary log code and sub-code. These messages are displayed on stderr and indicate various levels of problems.

### Parameters
- `LOG_CODE`: Error level (`kError`, `kFatal`, `kWarning`)
- `format_string`: Printf-style format string  
- `...args`: Arguments for the format string

### Output Format
```
filepath:line LEVEL thread_id function_name message
```

### Examples

#### Fatal Errors (Program Termination)
```cpp
HLOG(kFatal, "Could not find this allocator type");
// Output: /path/to/file.cc:63 FATAL 12345 init_allocator Could not find this allocator type
// Program exits after this message

HLOG(kFatal, "Failed to find the memory allocator?");
HLOG(kFatal, "Exception: {}", e.what());
```

#### Non-Fatal Errors
```cpp
HLOG(kError, "shm_open failed: {}", err_buf);
// Output: /path/to/file.cc:66 ERROR 12345 open_shared_memory shm_open failed: Permission denied

HLOG(kError, "Failed to generate key");
```

#### System/Hardware Errors
```cpp
// CUDA error handling
HLOG(kFatal, "CUDA Error {}: {}", cudaErr, cudaGetErrorString(cudaErr));

// HIP error handling  
HLOG(kFatal, "HIP Error {}: {}", hipErr, hipGetErrorString(hipErr));
```

## Advanced Features

### Periodic Logging
For messages that might be called frequently, use `HILOG_PERIODIC` to limit output frequency:

```cpp
HILOG_PERIODIC(kInfo, unique_id, interval_seconds, "Status update: {}", status);
```

### Environment Configuration

#### Disabling Log Codes
Set `HSHM_LOG_EXCLUDE` to a comma-separated list of log codes to disable:
```bash
export HSHM_LOG_EXCLUDE="251,252"  # Disable kInfo and kWarning
```

#### Log File Output
Set `HSHM_LOG_OUT` to write logs to a file (in addition to console):
```bash
export HSHM_LOG_OUT="/tmp/hermes_shm.log"
```

### Debug Builds
- In release builds: `kDebug` is defined as -1, and debug logs are compiled out
- In debug builds: `kDebug` is defined as 255, and debug logs are active

## Best Practices

1. **Use appropriate log levels**:
   - `HLOG(kInfo, ...)` for normal operational messages
   - `HLOG(kError, ...)` for recoverable errors
   - `HLOG(kFatal, ...)` for unrecoverable errors that should terminate the program

2. **Include context in error messages**:
   ```cpp
   HLOG(kError, "Failed to allocate {} bytes: {}", size, strerror(errno));
   ```

3. **Use meaningful sub-codes** for `HILOG` to categorize different types of information

4. **Format structured data consistently**:
   ```cpp
   HLOG(kInfo, "operation={},duration_ms={},status={}", op_name, duration, status);
   ```

5. **Avoid logging in tight loops** - use `HILOG_PERIODIC` instead

## Thread Safety

The logging system is thread-safe and automatically includes thread IDs in log output, making it suitable for multi-threaded applications.

## Performance Considerations

- Log messages are formatted only when the log level is enabled
- Disabled log codes (via `HSHM_LOG_EXCLUDE`) have minimal runtime overhead
- Debug logs have zero overhead in release builds due to compile-time optimization