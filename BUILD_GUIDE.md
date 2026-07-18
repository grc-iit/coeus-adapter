# COEUS-Adapter Build Guide

Complete guide for building COEUS-Adapter against **clio-core** (IOWarp's core:
Chimaera runtime + Context-Transfer-Engine), which is the backbone I/O engine.

> **A note on naming**: Hermes is **no longer used** - all I/O goes through
> clio-core's CTE. The names `hermes_engine` (library), `HermesEngine` (class),
> and `PluginName=hermes` (ADIOS2 XML) are retained from the original
> Hermes-based implementation so existing application configs keep working.

## Table of Contents

1. [Prerequisites](#prerequisites)
2. [Installing Dependencies](#installing-dependencies)
3. [Building COEUS-Adapter](#building-coeus-adapter)
4. [Using the Plugin](#using-the-plugin)
5. [Configuration](#configuration)
6. [Testing](#testing)
7. [Troubleshooting](#troubleshooting)

## Prerequisites

### System Requirements

- **OS**: Linux (Ubuntu 20.04+, CentOS 8+, or similar)
- **Compiler**: GCC >= 11 with C++20 coroutine support
  (the plugin and ChiMods are built with `-std=c++20 -fcoroutines`
  because Chimaera task bodies are C++20 coroutines)
- **CMake**: >= 3.20
- **MPI**: OpenMPI or MPICH

### Required Dependencies

All of these are found via `find_package` in `CMakeLists.txt`:

| Dependency | Provides | Typical source |
|---|---|---|
| **iowarp-core** | Chimaera runtime, CTE (tiered blob store), transport primitives - the entire clio-core stack | `spack install iowarp@main` |
| **ADIOS2** | Plugin engine interface, derived variables | Spack or source build (must be an *installed* tree, see [Troubleshooting](#troubleshooting)) |
| **MPI** (C, CXX) | Communication | `spack install openmpi` |
| **yaml-cpp** | Variable/operation config parsing | comes with the iowarp spack env |
| **SQLite3** | Metadata database | system or spack |
| **OpenMP** | Derived-variable computation | compiler |
| **GTest** | Unit tests | system or spack |

Bundled in `external_libraries/` (no install needed): spdlog, cereal, rapidjson.

Optional:
- **Catalyst 2** - in-situ visualization (`-DCOEUS_ENABLE_CATALYST=ON`, or auto-detected)

## Installing Dependencies

### Using Spack (recommended)

The `iowarp` Spack package lives in clio-core's own Spack repo:

```bash
# Load Spack
source /path/to/spack/share/spack/setup-env.sh

# Add the IOWarp Spack repo (ships inside clio-core)
git clone https://github.com/iowarp/clio-core.git
spack repo add clio-core/installers/spack

# Install and load
spack install iowarp@main
spack install openmpi
spack load iowarp@main
spack load openmpi
```

`spack load iowarp@main` puts `iowarp-coreConfig.cmake` (and yaml-cpp, sqlite,
etc. from its dependency tree) on `CMAKE_PREFIX_PATH` automatically.

> **Tip**: `spack load` can take several minutes on large installs. To cache the
> environment for fast reuse:
> ```bash
> spack load --sh iowarp@main openmpi > ~/coeus_env.sh
> # later, in any shell:
> source ~/coeus_env.sh
> ```

### ADIOS2

COEUS needs an ADIOS2 build with derived-variable support. Either:

```bash
spack install adios2
spack load adios2
```

or point CMake at your own **installed** ADIOS2 tree:

```bash
export CMAKE_PREFIX_PATH="/path/to/adios2/install:$CMAKE_PREFIX_PATH"
```

### Verify iowarp-core is discoverable

```bash
# Should print the package config location
find $(spack location -i iowarp@main) -name "iowarp-coreConfig.cmake"
```

## Building COEUS-Adapter

```bash
cd coeus-adapter
mkdir build && cd build

cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -Dmeta_enabled=ON \
    -Ddebug_mode=OFF

cmake --build . --parallel $(nproc)
```

**CMake options:**

| Option | Default | Effect |
|---|---|---|
| `-DCMAKE_BUILD_TYPE` | `Release` | `Debug` or `Release` |
| `-Dmeta_enabled=ON` | `OFF` | Enable metadata collection (SQLite via the `coeus_mdm` ChiMod) |
| `-Ddebug_mode=ON` | `OFF` | Verbose engine logging |
| `-DCOEUS_ENABLE_CATALYST=ON` | `OFF` (auto-detects) | Catalyst 2 + Fides in-situ visualization |
| `-DCMAKE_INSTALL_PREFIX` | `/usr/local` | Install destination |

**Verify the build:**

```bash
ls -lh bin/libhermes_engine.so   # the ADIOS2 plugin
ldd bin/libhermes_engine.so      # check no missing libraries
```

**Install (optional):**

```bash
cmake --install . --prefix /your/prefix
```

Built artifacts:
- `libhermes_engine.so` - the ADIOS2 plugin engine (main deliverable)
- `libcoeus_coeus_mdm.so`, `libcoeus_rankConsensus.so` - ChiMods loaded by the
  Chimaera runtime (see `tasks/`)

## Using the Plugin

Applications select COEUS in their ADIOS2 XML - no code changes required:

```xml
<io name="SimulationOutput">
    <engine type="Plugin">
        <parameter key="PluginName" value="hermes" />
        <parameter key="PluginLibrary" value="hermes_engine" />
    </engine>
</io>
```

Make sure the plugin is findable at runtime:

```bash
export ADIOS2_PLUGIN_PATH=/path/to/coeus-adapter/build/bin
export LD_LIBRARY_PATH=/path/to/coeus-adapter/build/bin:$LD_LIBRARY_PATH
```

See `test/jarvis/jarvis_coeus/jarvis_coeus/adios2_gray_scott/config/` for
complete working examples.

## Configuration

### CTE configuration

The CTE (tiered storage) is configured through the Chimaera runtime that hosts
it. A sample tier config is in `config/cte_config.yaml`. Deployment via
Jarvis pipelines (`test/jarvis/jarvis_coeus/pipelines/`) handles this
automatically.

### Runtime configuration

The Chimaera runtime must be running before applications open the engine.
With Jarvis this is the `chimaera_run` pipeline stage; manual deployments set:

```bash
export WRP_RUNTIME_CONF=/path/to/chimaera_config.yaml
```

## Testing

```bash
cd build
ctest -V                 # unit + integration tests
```

Application-level tests are driven through Jarvis pipelines - see
`test/jarvis/README.md` and `test/jarvis/jarvis_coeus/pipelines/`.

## Troubleshooting

### `CMAKE_C_COMPILER not set, after EnableLanguage`

Seen when `find_package(iowarp-core)` transitively pulls HDF5/MPI config tests.
Pass the compilers explicitly:

```bash
cmake .. -DCMAKE_C_COMPILER=$(which gcc) -DCMAKE_CXX_COMPILER=$(which g++) ...
```

### `fatal error: ../cxx/Variable.h: No such file or directory`

You pointed `CMAKE_PREFIX_PATH` at an ADIOS2 **build tree**. The build tree's
headers use relative includes that only resolve after installation. Run
`make install` in your ADIOS2 build and point at the **install** prefix instead.

### CMake cannot find `iowarp-core`

```bash
# Confirm the package exists
find $(spack location -i iowarp@main) -name "iowarp-coreConfig.cmake"

# Point CMake at it directly if needed
cmake .. -Diowarp-core_DIR=$(spack location -i iowarp@main)/lib/cmake/iowarp-core
```

### Missing libraries at runtime

```
error while loading shared libraries: libwrp_cte_core_client.so
```

```bash
export LD_LIBRARY_PATH=$(spack location -i iowarp@main)/lib:$LD_LIBRARY_PATH
```

### Engine hangs at Open / CTE errors

The Chimaera runtime is not up, or the ChiMods aren't found. Check that:
1. The runtime daemon is running (Jarvis `chimaera_run` stage).
2. `libcoeus_coeus_mdm.so` / `libcoeus_rankConsensus.so` are on the module
   search path so the runtime can load them.

## Additional Resources

- [README](README.md) - project overview and supported applications
- [Installation Guide](install.md) - end-to-end Spack-based install
- [Source Code Analysis](SOURCE_CODE_ANALYSIS.md) - architecture, and exactly
  how COEUS depends on clio-core
- Historical planning docs are archived in [docs/archive/](docs/archive/)
