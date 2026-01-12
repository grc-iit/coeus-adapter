# GoogleTest Conflict Analysis and Fix Plan

## Problem Analysis

### Root Cause
The build is encountering **class redefinition errors** because GoogleTest headers are being included from **two different sources**:

1. **CMake-fetched GoogleTest** (via FetchContent):
   - Location: `/home/iowarp/coeus-adapter/build/_deps/googletest-src/googletest/include/`
   - Version: Commit `f8d7d77c06936315286eb55f8de22cd23c188571`

2. **System-installed GoogleTest** (from miniconda3):
   - Location: `/home/iowarp/miniconda3/include/gtest/`
   - Different version with incompatible API

### Why It Happens

1. **Include Path Ordering**: The compiler searches include directories in a specific order:
   - System include paths (including miniconda3) are searched first
   - CMake-fetched GoogleTest headers are found later
   - This causes mixed includes from both sources

2. **Version Mismatch**: The two GoogleTest installations have:
   - Different API signatures (`GTEST_FLAG_GET` vs `GTEST_FLAG`)
   - Different class definitions
   - Incompatible internal structures

3. **Compilation Flow**:
   ```
   gtest-all.cc includes gtest.h
   → Finds system version from miniconda3 (first in path)
   → Then includes internal headers
   → Finds CMake-fetched version (different location)
   → Result: Redefinition errors
   ```

## Fix Plan

### Option 1: Exclude System GoogleTest from Include Path (Recommended)
**Pros**: Clean, ensures only one version is used
**Cons**: Requires compiler flag manipulation

**Implementation**:
- Add `-isystem` exclusion for miniconda3 gtest directory
- Or use `-I` with proper ordering to prioritize CMake-fetched version

### Option 2: Use System GoogleTest Instead of FetchContent
**Pros**: Simpler, uses existing installation
**Cons**: Version may not match requirements, less control

**Implementation**:
- Remove FetchContent for GoogleTest
- Use `find_package(GTest REQUIRED)` instead
- Link against system-installed version

### Option 3: Isolate GoogleTest Build (Best for Reliability)
**Pros**: Complete isolation, no conflicts
**Cons**: More complex setup

**Implementation**:
- Configure GoogleTest to use isolated include paths
- Set `CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES` to exclude system paths
- Use target-specific include directories

## Recommended Solution: Option 3 (Isolated Build)

This provides the most reliable solution by ensuring GoogleTest is completely isolated from system installations.

## Implementation Applied

The fix implements the following strategy:

1. **Early Include Path Configuration**: Before `FetchContent_MakeAvailable`, we populate GoogleTest and add its include directories to `CMAKE_CXX_FLAGS` to ensure they're found first.

2. **Target-Specific Configuration**: After GoogleTest is configured, we set `SYSTEM BEFORE` include directories on all GoogleTest targets to prioritize the fetched version over system installations.

3. **Warning Suppression**: Added `-Wno-undef` to suppress warnings about undefined macros (common with multiple installations).

### Key Changes:
- Use `FetchContent_GetProperties` and `FetchContent_Populate` before `MakeAvailable` to configure include paths early
- Add fetched GoogleTest include directories to `CMAKE_CXX_FLAGS` so they're searched before system paths
- Set `SYSTEM BEFORE` on all GoogleTest target include directories to ensure priority
- Suppress `-Wno-undef` warnings that occur with multiple GoogleTest installations

### Testing:
After applying this fix, rebuild from scratch:
```bash
rm -rf build
mkdir build && cd build
cmake ..
make
```

If issues persist, consider:
- Removing miniconda3 from `CMAKE_PREFIX_PATH` during build
- Using `-DCMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES=OFF` to exclude system paths
- Or switching to `find_package(GTest)` if system version is compatible

