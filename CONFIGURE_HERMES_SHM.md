# Configuring CMake to Find Hermes-Shm

This guide explains how to tell CMake where hermes-shm is installed.

## Step 1: Install Hermes-Shm

After building hermes-shm, you need to install it:

```bash
cd ~/core/context-transport-primitives/build
make install
```

**Note**: If you haven't specified an install prefix, it will install to `/usr/local` by default. To install to a custom location:

```bash
# Configure with custom install prefix
cd ~/core/context-transport-primitives
mkdir -p build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/path/to/your/install
make -j8
make install
```

## Step 2: Find the Install Location

After installation, hermes-shm will be installed to:
- **Default**: `/usr/local` (if no prefix specified)
- **Custom**: Whatever you set with `-DCMAKE_INSTALL_PREFIX`

The install structure should look like:
```
/path/to/install/
├── include/
│   └── hermes_shm/          # Headers
├── lib/
│   ├── libhermes_shm*.so    # Libraries
│   └── cmake/
│       └── HermesShm/        # CMake config files
└── bin/                      # Binaries (if any)
```

## Step 3: Configure CMake to Find Hermes-Shm

### Method 1: Using CMAKE_PREFIX_PATH (Recommended)

Set `CMAKE_PREFIX_PATH` to include the hermes-shm install directory:

```bash
# If installed to /usr/local (default)
export CMAKE_PREFIX_PATH="/usr/local:$CMAKE_PREFIX_PATH"

# If installed to custom location
export CMAKE_PREFIX_PATH="/path/to/your/install:$CMAKE_PREFIX_PATH"

# Or combine multiple paths
export CMAKE_PREFIX_PATH="/usr/local:/path/to/hermes-shm:/path/to/other/deps"
```

Then configure coeus-adapter:
```bash
cd coeus-adapter
mkdir -p build && cd build
cmake ..
make -j8
```

### Method 2: Using HermesShm_DIR

Point CMake directly to the HermesShm config directory:

```bash
cd coeus-adapter
mkdir -p build && cd build
cmake .. \
    -DHermesShm_DIR=/path/to/install/lib/cmake/HermesShm
```

**Example** (if installed to `/usr/local`):
```bash
cmake .. -DHermesShm_DIR=/usr/local/lib/cmake/HermesShm
```

### Method 3: Using CMAKE_PREFIX_PATH in CMake Command

You can also set it directly in the cmake command:

```bash
cd coeus-adapter
mkdir -p build && cd build
cmake .. \
    -DCMAKE_PREFIX_PATH="/path/to/hermes-shm/install:/usr/local"
```

### Method 4: Using Environment Variables (Alternative)

Set multiple environment variables for comprehensive search:

```bash
# Set library paths
export LIBRARY_PATH="/path/to/install/lib:$LIBRARY_PATH"
export LD_LIBRARY_PATH="/path/to/install/lib:$LD_LIBRARY_PATH"

# Set include paths
export CPATH="/path/to/install/include:$CPATH"
export CXXFLAGS="-I/path/to/install/include $CXXFLAGS"

# Set CMake prefix path
export CMAKE_PREFIX_PATH="/path/to/install:$CMAKE_PREFIX_PATH"
```

## Step 4: Verify CMake Found Hermes-Shm

After running `cmake ..`, check the output for:

```
-- found hermes_shm at /path/to/install
```

Or if it's using as subdirectory:
```
-- using hermes_shm as subdirectory (target: cxx)
```

If you see an error like:
```
CMake Error: Hermes SHM (HSHM) not found
```

Then CMake couldn't find hermes-shm. Try:
1. Verify the install location
2. Check that `lib/cmake/HermesShm/` directory exists
3. Ensure `CMAKE_PREFIX_PATH` includes the install directory

## Complete Example

Here's a complete example assuming hermes-shm is installed to `/usr/local`:

```bash
# 1. Build and install hermes-shm
cd ~/core/context-transport-primitives
mkdir -p build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local
make -j8
sudo make install  # or make install if you have write permissions

# 2. Set environment (if not using /usr/local)
export CMAKE_PREFIX_PATH="/usr/local:$CMAKE_PREFIX_PATH"
export LD_LIBRARY_PATH="/usr/local/lib:$LD_LIBRARY_PATH"

# 3. Build coeus-adapter
cd ~/coeus-adapter
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j8
```

## Troubleshooting

### Problem: CMake can't find HermesShm

**Check 1**: Verify installation
```bash
# Check if CMake config exists
ls /path/to/install/lib/cmake/HermesShm/

# Should see files like:
# HermesShmConfig.cmake
# HermesShmConfigVersion.cmake
# HermesShmTargets.cmake
```

**Check 2**: Verify CMAKE_PREFIX_PATH
```bash
# In CMake output, look for:
# CMAKE_PREFIX_PATH: /path/to/install
```

**Check 3**: Try explicit path
```bash
cmake .. -DHermesShm_DIR=/path/to/install/lib/cmake/HermesShm -Wno-dev
```

### Problem: Libraries not found at runtime

**Solution**: Add to LD_LIBRARY_PATH
```bash
export LD_LIBRARY_PATH="/path/to/install/lib:$LD_LIBRARY_PATH"
```

Or update system library path:
```bash
echo "/path/to/install/lib" | sudo tee /etc/ld.so.conf.d/hermes-shm.conf
sudo ldconfig
```

### Problem: Headers not found

**Solution**: Verify include directory
```bash
# Check headers exist
ls /path/to/install/include/hermes_shm/

# Add to CMAKE_PREFIX_PATH or CPATH
export CPATH="/path/to/install/include:$CPATH"
```

## Quick Reference

```bash
# 1. Install hermes-shm
cd ~/core/context-transport-primitives/build
make install  # Installs to /usr/local by default

# 2. Set environment
export CMAKE_PREFIX_PATH="/usr/local:$CMAKE_PREFIX_PATH"
export LD_LIBRARY_PATH="/usr/local/lib:$LD_LIBRARY_PATH"

# 3. Build coeus-adapter
cd ~/coeus-adapter
mkdir -p build && cd build
cmake ..
make -j8
```

## For Your Specific Case

Based on your build output from `~/core/context-transport-primitives/build`, here's what to do:

```bash
# 1. Install hermes-shm (if not already done)
cd ~/core/context-transport-primitives/build
make install

# 2. Find where it was installed
# Check the install prefix used during cmake configuration
# If you didn't specify one, it's likely /usr/local

# 3. Set CMAKE_PREFIX_PATH
export CMAKE_PREFIX_PATH="/usr/local:$CMAKE_PREFIX_PATH"
# Or if installed elsewhere:
# export CMAKE_PREFIX_PATH="/path/to/actual/install:$CMAKE_PREFIX_PATH"

# 4. Build coeus-adapter
cd ~/coeus-adapter
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j8
```

If you want to check where hermes-shm was actually installed:

```bash
# Check if CMake config exists in common locations
find /usr/local -name "HermesShmConfig.cmake" 2>/dev/null
find ~ -name "HermesShmConfig.cmake" 2>/dev/null

# Or check the build directory for install_manifest.txt
cat ~/core/context-transport-primitives/build/install_manifest.txt
```

