# Alternative GoogleTest Fix Strategy

## The Core Problem

The issue is that when GoogleTest compiles its own source files (like `gtest-all.cc`), the compiler finds the system GoogleTest headers from miniconda3 **before** the fetched version, even with `-I` flags.

## Root Cause Analysis

1. **Include Path Search Order**:
   ```
   System include paths (from compiler defaults)
   → miniconda3/include (from environment)
   → -I flags (our fetched GoogleTest)
   → System paths again
   ```

2. **Why `-I` isn't enough**: Even though `-I` adds directories before system paths, if the system path is in the compiler's default search path, it may still be found first.

3. **Why `-idirafter` doesn't help**: This only affects system include directories added via `-isystem`, not the compiler's default system include paths.

## Alternative Solutions

### Solution 1: Use System GoogleTest (Simplest)
**If the system GoogleTest version is compatible**, just use it:

```cmake
# Remove FetchContent for GoogleTest
# find_package(GTest REQUIRED)
# target_link_libraries(your_target GTest::gtest GTest::gtest_main)
```

### Solution 2: Temporarily Remove miniconda3 from PATH
**During build only**:
```bash
# Save original PATH
export PATH_BACKUP=$PATH
# Remove miniconda3 from PATH
export PATH=$(echo $PATH | tr ':' '\n' | grep -v miniconda3 | tr '\n' ':')
# Build
cmake .. && make
# Restore PATH
export PATH=$PATH_BACKUP
```

### Solution 3: Use Different GoogleTest Version
**Try a different commit** that might be more compatible:
```cmake
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0  # Try a release tag instead
)
```

### Solution 4: Build GoogleTest in Isolation (Most Reliable)
**Create a separate build directory for GoogleTest**:
```cmake
# Build GoogleTest separately with isolated environment
# Then link against it
```

### Solution 5: Patch GoogleTest Source (Nuclear Option)
**Modify GoogleTest source to use absolute includes**:
- Change `#include <gtest/gtest.h>` to `#include "gtest/gtest.h"` in GoogleTest's own source files
- This forces relative includes which won't find system version

## Recommended Immediate Fix

**Try Solution 1 first** - check if system GoogleTest works:
```bash
# Check if system GoogleTest is available
pkg-config --modversion gtest
# Or
find /usr -name "gtest.h" 2>/dev/null
find $CONDA_PREFIX -name "gtest.h" 2>/dev/null
```

If system version exists and is compatible, use it instead of FetchContent.

