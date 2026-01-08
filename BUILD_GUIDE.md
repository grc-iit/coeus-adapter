# Coeus-Adapter Build Guide

Complete guide for building coeus-adapter with Chimaera Runtime and Context-Transfer-Engine (CTE) support.

## Table of Contents

1. [Prerequisites](#prerequisites)
2. [Installing Dependencies](#installing-dependencies)
3. [Building Coeus-Adapter](#building-coeus-adapter)
4. [Configuration](#configuration)
5. [Testing](#testing)
6. [Troubleshooting](#troubleshooting)

## Prerequisites

### System Requirements

- **OS**: Linux (Ubuntu 20.04+, CentOS 8+, or similar)
- **Compiler**: C++17 compatible (GCC >= 9, Clang >= 10)
- **CMake**: >= 3.20 (for CTE support)
- **MPI**: OpenMPI or MPICH
- **Python**: 3.7+ (optional, for Python bindings)

### Required Dependencies

1. **HermesShm** - Shared memory framework
2. **Chimaera Core** - Context-Runtime framework
3. **Chimaera Admin** - Admin ChiMod for pool management
4. **Context-Transfer-Engine (CTE)** - I/O placement engine
5. **Hermes** - Storage backend (legacy, still needed)
6. **ADIOS2** - I/O library
7. **yaml-cpp** - YAML configuration parsing
8. **SQLite3** - Database support

## Installing Dependencies

### Option 1: Using Spack (Recommended)

If you're using Spack for dependency management:

```bash
# Load Spack environment
source /path/to/spack/share/spack/setup-env.sh

# Install HermesShm (if not already installed)
spack install hermes-shm

# Install Chimaera Core and Admin
# Note: These may need to be built from source if not in Spack
# See Option 2 below

# Install CTE
# Note: CTE may need to be built from source
# See Option 2 below

# Install other dependencies
spack install adios2
spack install yaml-cpp
spack install sqlite
spack install openmpi

# Load dependencies
spack load hermes-shm
spack load adios2
spack load yaml-cpp
spack load sqlite
spack load openmpi
```

### Option 2: Building from Source

#### 1. Install HermesShm

```bash
git clone https://github.com/HDFGroup/hermes-shm.git
cd hermes-shm
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local
make -j$(nproc)
sudo make install
```

#### 2. Install Chimaera Runtime

```bash
# Navigate to context-runtime directory (if in coeus-adapter repo)
cd context-runtime

# Or clone separately:
# git clone https://github.com/iowarp/iowarp-runtime.git
# cd iowarp-runtime

# Configure with CMake preset
cmake --preset release
# Or manually:
# mkdir build && cd build
# cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local

# Build
cmake --build build --parallel $(nproc)

# Install
cmake --install build --prefix /usr/local
# Or with sudo if needed:
# sudo cmake --install build --prefix /usr/local
```

**Verify Chimaera Installation:**
```bash
# Check if libraries are installed
ls /usr/local/lib/libchimaera*.so
ls /usr/local/lib/libchimaera_admin*.so

# Check if headers are available
ls /usr/local/include/chimaera/
```

#### 3. Install Context-Transfer-Engine (CTE)

```bash
# Navigate to context-transfer-engine directory (if in coeus-adapter repo)
cd context-transfer-engine

# Or clone separately:
# git clone <cte-repository-url>
# cd context-transfer-engine

# Configure with CMake
mkdir build && cd build
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr/local \
    -DCMAKE_PREFIX_PATH=/usr/local

# Build
make -j$(nproc)

# Install
sudo make install
```

**Verify CTE Installation:**
```bash
# Check if libraries are installed
ls /usr/local/lib/libwrp_cte_core*.so

# Check if headers are available
ls /usr/local/include/wrp_cte/core/
```

#### 4. Install Other Dependencies

**ADIOS2:**
```bash
# Using Spack (recommended)
spack install adios2

# Or from source
git clone https://github.com/ornladios/ADIOS2.git
cd ADIOS2
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local
make -j$(nproc)
sudo make install
```

**yaml-cpp:**
```bash
# Using package manager
sudo apt-get install libyaml-cpp-dev  # Ubuntu/Debian
sudo yum install yaml-cpp-devel       # RHEL/CentOS

# Or from source
git clone https://github.com/jbeder/yaml-cpp.git
cd yaml-cpp
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local
make -j$(nproc)
sudo make install
```

**SQLite3:**
```bash
# Usually pre-installed, but if needed:
sudo apt-get install libsqlite3-dev  # Ubuntu/Debian
sudo yum install sqlite-devel        # RHEL/CentOS
```

## Building Coeus-Adapter

### Step 1: Set Environment Variables

Set `CMAKE_PREFIX_PATH` to include all dependency installation paths:

```bash
export CMAKE_PREFIX_PATH="/usr/local:/path/to/hermes-shm:/path/to/other/deps"
```

If using Spack:
```bash
# Spack automatically sets CMAKE_PREFIX_PATH when loading packages
spack load hermes-shm
spack load adios2
spack load yaml-cpp
spack load sqlite
spack load openmpi
```

### Step 2: Configure CMake

```bash
cd coeus-adapter
mkdir build && cd build

# Basic configuration
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr/local

# With optional features
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr/local \
    -Dmeta_enabled=ON \
    -Ddebug_mode=ON

# If dependencies are in non-standard locations
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr/local \
    -DCMAKE_PREFIX_PATH="/usr/local:/custom/path/to/deps" \
    -Dchimaera-core_DIR=/path/to/chimaera-core/lib/cmake/chimaera-core \
    -Dchimaera-admin_DIR=/path/to/chimaera-admin/lib/cmake/chimaera-admin \
    -Dwrp_cte_core_DIR=/path/to/wrp_cte_core/lib/cmake/wrp_cte_core
```

**CMake Configuration Options:**
- `-DCMAKE_BUILD_TYPE`: `Debug` or `Release` (default: `Release`)
- `-Dmeta_enabled=ON`: Enable metadata features
- `-Ddebug_mode=ON`: Enable debug logging
- `-DCMAKE_INSTALL_PREFIX`: Installation directory (default: `/usr/local`)

### Step 3: Build

```bash
# Build with all available cores
make -j$(nproc)

# Or specify number of cores
make -j8
```

### Step 4: Verify Build

Check that libraries are built:
```bash
ls -lh bin/libhermes_engine.so
```

Check for any missing dependencies:
```bash
ldd bin/libhermes_engine.so
```

### Step 5: Install (Optional)

```bash
sudo make install
```

This installs:
- Libraries to `${CMAKE_INSTALL_PREFIX}/lib`
- Headers to `${CMAKE_INSTALL_PREFIX}/include`
- Binaries to `${CMAKE_INSTALL_PREFIX}/bin`

## Configuration

### 1. CTE Configuration

Create or update the CTE configuration file:

```bash
# Copy default configuration
cp config/cte_config.yaml /path/to/your/cte_config.yaml

# Edit as needed
nano /path/to/your/cte_config.yaml
```

Set the configuration path via environment variable:
```bash
export CTE_CONFIG=/path/to/your/cte_config.yaml
```

Or specify in your application code.

**Example CTE Configuration** (`config/cte_config.yaml`):
```yaml
worker_count: 4

storage:
  - path: "/tmp/cte_primary"
    bdev_type: "file"
    capacity_limit: "10GB"
    score: 0.9
  
  - path: "/tmp/cte_cache"
    bdev_type: "ram"
    capacity_limit: "2GB"
    score: 1.0

dpe:
  dpe_type: "max_bw"
```

### 2. Chimaera Runtime Configuration

If using external Chimaera runtime (not embedded), configure it:

```bash
# Set Chimaera configuration path
export WRP_RUNTIME_CONF=/path/to/chimaera_config.yaml

# Or use default location
# Default: config/chimaera_default.yaml
```

### 3. Hermes Configuration (Legacy)

If using Hermes storage backend (fallback mode):

```bash
export HERMES_CONF=/path/to/hermes_config.yaml
```

## Testing

### 1. Unit Tests

```bash
cd build
ctest -V
```

### 2. Integration Tests

```bash
# Run ADIOS2 integration tests
cd test/integration
./run_tests.sh
```

### 3. Manual Verification

Create a simple test program:

```cpp
#include <coeus/HermesEngine.h>
#include <comms/Hermes.h>

int main() {
    auto hermes = std::make_shared<coeus::Hermes>();
    if (hermes->connect()) {
        std::cout << "CTE initialized successfully!" << std::endl;
        return 0;
    } else {
        std::cerr << "Failed to initialize CTE" << std::endl;
        return 1;
    }
}
```

Compile and run:
```bash
g++ -o test_cte test_cte.cc \
    -I/usr/local/include \
    -L/usr/local/lib \
    -lhermes_engine \
    -lwrp_cte_core_client \
    -lchimaera_cxx \
    -lchimaera_admin_client

export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH
./test_cte
```

## Troubleshooting

### CMake Cannot Find Packages

**Problem**: `find_package(chimaera-core REQUIRED)` fails

**Solution**:
```bash
# Set CMAKE_PREFIX_PATH
export CMAKE_PREFIX_PATH="/usr/local:$CMAKE_PREFIX_PATH"

# Or specify directly in CMake
cmake .. -Dchimaera-core_DIR=/usr/local/lib/cmake/chimaera-core
```

### Missing Libraries at Runtime

**Problem**: `error while loading shared libraries: libwrp_cte_core_client.so`

**Solution**:
```bash
# Add to LD_LIBRARY_PATH
export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH

# Or update system library path
echo "/usr/local/lib" | sudo tee /etc/ld.so.conf.d/coeus.conf
sudo ldconfig
```

### CTE Initialization Fails

**Problem**: CTE client initialization returns false

**Solution**:
1. Check CTE configuration file exists and is valid:
   ```bash
   cat $CTE_CONFIG
   ```

2. Verify Chimaera runtime is running (if using external runtime):
   ```bash
   # Start Chimaera runtime if needed
   chimaera_start_runtime
   ```

3. Check storage paths exist and are writable:
   ```bash
   mkdir -p /tmp/cte_primary /tmp/cte_cache
   chmod 777 /tmp/cte_primary /tmp/cte_cache
   ```

### Build Errors Related to hermes::BlobId

**Problem**: Compilation errors about `hermes::BlobId` structure

**Solution**: The `hermes::BlobId` structure may need adjustment in `src/CTEBucket.cc`. Check the actual structure definition in Hermes headers and update the `GenerateBlobId()` method accordingly.

### Linker Errors

**Problem**: Undefined references to CTE or Chimaera symbols

**Solution**: Ensure all required libraries are linked:
```cmake
target_link_libraries(hermes_engine
    wrp_cte::core_client
    chimaera::cxx
    chimaera::admin_client
    # ... other libraries
)
```

## Quick Reference

### Build Commands Summary

```bash
# 1. Set environment
export CMAKE_PREFIX_PATH="/usr/local"
export CTE_CONFIG="config/cte_config.yaml"
export LD_LIBRARY_PATH="/usr/local/lib:$LD_LIBRARY_PATH"

# 2. Configure
cd coeus-adapter
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release

# 3. Build
make -j$(nproc)

# 4. Test
ctest -V

# 5. Install (optional)
sudo make install
```

### Verification Checklist

- [ ] Chimaera Core installed and found by CMake
- [ ] Chimaera Admin installed and found by CMake
- [ ] CTE Core installed and found by CMake
- [ ] All libraries build successfully
- [ ] CTE configuration file exists and is valid
- [ ] Runtime libraries are in LD_LIBRARY_PATH
- [ ] Tests pass

## Additional Resources

- [Chimaera Runtime Documentation](../context-runtime/README.md)
- [CTE Documentation](../context-transfer-engine/docs/cte.md)
- [Migration Summary](MIGRATION_SUMMARY.md)
- [IO Migration Status](IO_MIGRATION_STATUS.md)

## Support

For issues or questions:
1. Check the troubleshooting section above
2. Review the migration documentation
3. Check dependency installation paths
4. Verify configuration files

