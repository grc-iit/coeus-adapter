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

The fix implements a **two-tier strategy**:

### Strategy 1: Prefer System GoogleTest (If Available)
- First tries to find system GoogleTest using `find_package(GTest)`
- If found and compatible, uses system version (avoids all conflicts)
- This is the cleanest solution when system GoogleTest is available

### Strategy 2: Isolate Fetched GoogleTest (Fallback)
- If system GoogleTest not found, uses FetchContent
- Uses `-I` flags to prioritize fetched GoogleTest headers
- Sets include directories with `BEFORE` to ensure priority
- Suppresses warnings with `-Wno-undef`

### Key Changes:
1. **Try system GoogleTest first**: `find_package(GTest QUIET)` before FetchContent
2. **Use `-I` flags**: Explicitly add fetched GoogleTest include directory with `-I` (searched before system paths)
3. **Set `BEFORE` include directories**: Ensures fetched headers are searched first
4. **Warning suppression**: `-Wno-undef` for macro warnings

### If Issues Persist:

**Option A: Use System GoogleTest Only**
```bash
# Before building, ensure system GoogleTest is available
sudo apt-get install libgtest-dev  # Ubuntu/Debian
# Or
conda install -c conda-forge gtest  # If using conda

# Then rebuild - CMake will detect and use system version
```

**Option B: Exclude miniconda3 from Build Environment**
```bash
# Temporarily remove conda from PATH during build
export PATH=$(echo $PATH | tr ':' '\n' | grep -v miniconda3 | tr '\n' ':')
export CPLUS_INCLUDE_PATH=""
export C_INCLUDE_PATH=""
cmake .. && make
```

**Option C: Clean Build with Isolated Environment**
```bash
rm -rf build
mkdir build && cd build
# Build without conda in environment
env -u CONDA_PREFIX -u MINICONDA3 cmake ..
make
```

### Testing:
After applying this fix, rebuild from scratch:
```bash
rm -rf build
mkdir build && cd build
cmake ..
make
```

The build should now either:
- Use system GoogleTest (if available) - **no conflicts**
- Or use fetched GoogleTest with proper isolation - **minimal conflicts**

