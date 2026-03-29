# COEUS-Adapter Source Code Analysis

## 1. Project Overview

**COEUS-Adapter** is an ADIOS2 plugin engine that bridges ADIOS2 (a high-performance I/O framework for scientific computing) with the IOWarp storage ecosystem. It provides multi-tiered I/O buffering, in-situ derived variable computation, metadata management, and optional in-situ visualization via Catalyst/Fides.

- **Language**: C++17/C++20 (coroutines for Chimaera)
- **License**: BSD 3-Clause (Illinois Institute of Technology)
- **Build System**: CMake 3.10+
- **Current Branch**: `iowarp` (migrating from legacy Hermes to CTE/IOWarp)

---

## 2. Architecture

```
+----------------------------+
|   Scientific Application   |
|   (WRF, LAMMPS, Gray-Scott,|
|    Incompact3D, OpenFOAM)  |
+-------------+--------------+
              |  ADIOS2 API (Put/Get/BeginStep/EndStep)
              v
+----------------------------+
|   ADIOS2 Plugin Interface  |
|   (PluginEngineInterface)  |
+-------------+--------------+
              |
              v
+----------------------------+       +---------------------+
|    HermesEngine (coeus)    |<----->| Catalyst/Fides      |
|    src/hermes_engine.cc    |       | (in-situ viz, opt.) |
+---+--------+--------+-----+       +---------------------+
    |        |        |
    v        v        v
+-------+ +------+ +----------+
|CTEHermes| |SQLite| |ChiMods  |
|(IHermes) | (meta)| |          |
+---+---+ +------+ +----+-----+
    |                    |
    v                    v
+-------------------+ +-------------------+
| CTE (Context      | | Chimaera Runtime  |
|  Transfer Engine)  | | (Task Execution)  |
| wrp_cte::core     | | Pools, IPC, Mods  |
+-------------------+ +-------------------+
    |
    v
+---------------------------------+
| HermesShm (Context Transport    |
|  Primitives) - shared memory,   |
|  data structures, networking    |
+---------------------------------+
```

### Core Data Flow

1. **Write Path**: Application calls `adios2::Put()` -> `HermesEngine::DoPutSync_/DoPutDeferred_` -> `CTEHermes::Put()` -> CTE blob storage. Metadata is generated and stored via SQLite and the `coeus_mdm` ChiMod.

2. **Read Path**: Application calls `adios2::Get()` -> `HermesEngine::DoGetSync_/DoGetDeferred_` -> `CTETagClient::Get()` -> retrieves blob data from CTE.

3. **Derived Variables**: At `EndStep()`, `ComputeDerivedVariables()` reads input blobs from CTE, applies ADIOS2 derived variable expressions (curl, Q-criterion, hash, etc.), and writes results back via `PutDerived()`.

---

## 3. Directory Structure

```
coeus-adapter/
├── src/                          # Core engine implementation
│   ├── hermes_engine.cc          # Main ADIOS2 plugin engine (1041 lines)
│   └── CMakeLists.txt            # Build config for hermes_engine library
├── include/                      # Header files
│   ├── coeus/                    # Engine headers
│   │   ├── HermesEngine.h        # Main engine class definition
│   │   ├── Container.h           # Derived variable container
│   │   ├── ContainerManager.h    # Container orchestration
│   │   └── MetadataSerializer.h  # Metadata serialization
│   ├── comms/                    # Communication layer
│   │   ├── interfaces/
│   │   │   ├── IHermes.h         # Abstract I/O interface
│   │   │   └── ITag.h            # Abstract tag/blob interface
│   │   ├── CTEHermes.h/.cc       # CTE implementation of IHermes
│   │   ├── CTETagClient.h/.cc    # CTE implementation of ITag
│   │   └── MPI.h                 # MPI wrapper
│   └── common/                   # Shared utilities
│       ├── SQlite.h              # SQLite metadata wrapper
│       ├── DbOperation.h         # Database operation types
│       ├── MetadataStructs.h     # Core metadata structures
│       ├── VariableMetadata.h    # Variable metadata + serialization
│       ├── YAMLParser.h          # YAML config parser
│       ├── JSONParser.h          # JSON config parser
│       ├── ErrorCodes.h          # Error code definitions
│       ├── Tracer.h              # Debug tracing
│       ├── ThreadPool.h          # Thread pool utility
│       ├── CatalystHelper.h      # Catalyst in-situ viz integration
│       └── ClassLoader.h         # Dynamic library loader
├── tasks/                        # Chimaera ChiMod modules
│   ├── coeus_mdm/                # Metadata management ChiMod
│   │   ├── include/chimaera/coeus_mdm/
│   │   │   ├── coeus_mdm_client.h    # Client API (Create, Mdm_insert)
│   │   │   ├── coeus_mdm_runtime.h   # Runtime task handler
│   │   │   └── coeus_mdm_tasks.h     # Task definitions
│   │   └── src/                       # Implementation
│   └── rankConsensus/            # Rank assignment ChiMod
│       ├── include/chimaera/rankConsensus/
│       │   ├── rankConsensus_client.h # Client API (Create, GetRank)
│       │   ├── rankConsensus_runtime.h
│       │   └── rankConsensus_tasks.h
│       └── src/
├── test/                         # Tests and example applications
│   ├── unit/                     # Unit tests (GTest, SQLite, parsers, MPI)
│   ├── integration/              # Integration tests
│   ├── real_apps/                # Full application benchmarks
│   │   ├── gray-scott/           # Reaction-diffusion simulation
│   │   ├── hash_operator/        # Hashing operator tests
│   │   ├── io_comp/              # I/O comparison benchmarks
│   │   ├── metadata_comp/        # Metadata system comparisons
│   │   └── operator_comp/        # Operator comparison tests
│   └── jarvis/                   # Jarvis deployment pipelines
├── external_libraries/           # Vendored dependencies
│   ├── spdlog/                   # Logging library
│   ├── cereal/                   # Serialization library
│   └── rapidjson/                # JSON parsing library
├── CI/                           # CI/CD, Spack packages, Docker
├── context-transfer-engine/      # Git submodule: CTE (Hermes I/O)
├── context-runtime/              # Git submodule: Chimaera runtime
├── context-exploration-engine/   # Git submodule: CEE (data exploration API)
├── context-assimilation-engine/  # Git submodule: CAE (data ingestion)
├── context-transport-primitives/ # Git submodule: HermesShm (shared memory)
├── config/                       # Configuration files
├── .devcontainer/                # Docker dev container setup
└── .github/                      # GitHub Actions CI
```

---

## 4. Core Components

### 4.1 HermesEngine (`src/hermes_engine.cc`, `include/coeus/HermesEngine.h`)

The central class, inheriting from `adios2::plugin::PluginEngineInterface`. This is what ADIOS2 dynamically loads as a plugin engine.

**Key Responsibilities:**
- **Initialization**: Connects to Chimaera runtime (client mode), initializes CTE, creates `rankConsensus` and `coeus_mdm` pools, parses YAML operator/variable configs
- **Step Management**: `BeginStep()` creates a CTE tag per step/rank (`step_{N}_rank{R}`); `EndStep()` computes derived variables and cleans up tags
- **Data Put**: `DoPutSync_`/`DoPutDeferred_` write blobs to CTE via `CTEHermes::Put()`, generate metadata, and optionally forward data to Catalyst SST/Inline engine
- **Data Get**: `DoGetSync_`/`DoGetDeferred_` read blobs from CTE via `CTETagClient::Get()` with `memcpy` to user buffers
- **Derived Variables**: `ComputeDerivedVariables()` uses ADIOS2's `VariableDerived` expressions to compute in-situ quantities
- **Tier Management**: `Promote()`/`Demote()` prefetch/evict data across storage tiers
- **Catalyst Integration**: Optional in-situ visualization via Catalyst/Fides (Inline for single-node, SST for multi-node)

**Configuration Parameters** (set via ADIOS2 XML):
| Parameter | Description |
|-----------|-------------|
| `OPFile` | YAML file defining derived operations |
| `VarFile` | YAML file defining variable mappings |
| `ppn` | Processes per node |
| `limit` | Step limit |
| `lookahead` | Prefetch lookahead (default: 2) |
| `db_file` | SQLite database path for metadata |
| `Script` | Catalyst Python script path |
| `DataModel` | Catalyst/Fides JSON data model |
| `CatalystStream` | SST stream name (multi-node) |

### 4.2 Communication Layer (`include/comms/`)

#### IHermes Interface
Abstract interface for I/O operations:
- `connect()` - Initialize storage backend
- `GetTag(name)` - Create/get a named tag (container for blobs)
- `Put(name, size, data)` - Write blob data
- `Demote(tag, blob)` - Evict to lower tier
- `Prefetch(tag, blob)` - Prefetch to higher tier
- `tag` - Pointer to current `ITag` instance

#### ITag Interface
Abstract interface for blob operations within a tag:
- `Put(name, size, data)` - Write blob
- `Get(name)` -> `vector<uint8_t>` - Read blob
- `GetContainedBlobNames()` - List all blobs
- `GetBlobSize(name)` - Query blob size

#### CTEHermes (`CTEHermes.h/.cc`)
Concrete `IHermes` implementation using CTE (Context Transfer Engine). Inherits from both `IHermes` and `wrp_cte::core::Client`. Manages CTE pools, tag creation, and blob I/O.

#### CTETagClient (`CTETagClient.h/.cc`)
Concrete `ITag` implementation using CTE's client API directly. Handles per-tag blob Put/Get operations with a default blob score of 0.7 for data placement.

### 4.3 Chimaera ChiMods (`tasks/`)

#### coeus_mdm (Metadata Manager)
A Chimaera module that provides distributed metadata insertion via task-based execution:
- **Client API**: `Create(query, name, pool_id, db_path)`, `Mdm_insert(query, db_op)`
- **Runtime**: Handles `DbOperation` objects (InsertData, UpdateSteps, InsertDerivedData, CheckVariable)
- **Pool ID**: 8000

#### rankConsensus (Rank Assignment)
A Chimaera module for coordinated rank assignment across MPI processes:
- **Client API**: `Create(query, name, pool_id)`, `GetRank(query)` -> `u32`
- **Purpose**: Assigns unique ranks across distributed processes via the Chimaera runtime
- **Pool ID**: 8001

### 4.4 Metadata Management (`include/common/`)

#### SQLiteWrapper (`SQlite.h`)
SQLite-based metadata storage with four tables:
1. **Apps** - Tracks application names and total step counts
2. **BlobLocations** - Maps (step, rank, name) to (tag_name, blob_name)
3. **VariableMetadataTable** - Stores variable shape, start, count, type, derived flag
4. **derived_targets** - Stores derived quantity semantics (min/max values)

#### Key Data Structures
- **VariableMetadata**: Name, shape, start, count, constantShape, derived flag, dataType
- **BlobInfo**: tag_name + blob_name pair identifying CTE storage location
- **DbOperation**: Encapsulates metadata operations with step, rank, variable metadata, and blob info
- **derivedSemantics**: Min/max float values for derived quantities

### 4.5 Catalyst/Fides In-Situ Visualization (`include/common/CatalystHelper.h`)

Optional integration with ParaView Catalyst 2 for in-situ visualization:
- **Inline mode** (single-node): Data is passed in-process to Catalyst via ADIOS2 Inline engine
- **SST mode** (multi-node): Data is streamed via ADIOS2 SST engine to an external Catalyst reader
- Functions: `CatalystConfig()`, `CatalystInit()`, `CatalystExecute()`

---

## 5. IOWarp Submodules

The project includes five IOWarp ecosystem submodules:

| Submodule | Directory | Purpose |
|-----------|-----------|---------|
| **Context Transfer Engine (CTE)** | `context-transfer-engine/` | Multi-tiered I/O buffering system with adapters for POSIX, STDIO, MPI-IO, ADIOS2, HDF5 VFD, NVIDIA GDS |
| **Chimaera Runtime** | `context-runtime/` | Coroutine-based distributed task execution runtime with ChiMod system, IPC manager, pool manager |
| **HermesShm** | `context-transport-primitives/` | Shared memory data structures, allocators, networking (ZMQ), GPU support (CUDA/ROCm) |
| **Content Assimilation Engine (CAE)** | `context-assimilation-engine/` | Data ingestion from external sources (binary, HDF5, Globus) into the IOWarp ecosystem |
| **Context Exploration Engine (CEE)** | `context-exploration-engine/` | High-level API for querying, bundling, and managing IOWarp data contexts |

---

## 6. Build System and Dependencies

### External Dependencies
| Dependency | Purpose | Required |
|------------|---------|----------|
| `iowarp-core` | Unified IOWarp package (HermesShm, Chimaera, CTE) | Yes |
| `ADIOS2` | I/O framework, plugin host | Yes |
| `MPI` (C + CXX) | Distributed communication | Yes |
| `yaml-cpp` | YAML configuration parsing | Yes |
| `OpenMP` (C + CXX) | Parallel computation | Yes |
| `SQLite3` | Metadata storage | Yes |
| `GTest` | Unit testing | Yes |
| `Catalyst` | In-situ visualization | No |

### Vendored Libraries
- **spdlog** - Logging
- **cereal** - Binary serialization (used for metadata)
- **rapidjson** - JSON parsing

### Build Options
| Option | Default | Description |
|--------|---------|-------------|
| `meta_enabled` | OFF | Enable metadata collection (defines `Meta_enabled`) |
| `debug_mode` | OFF | Enable debug mode |
| `COEUS_ENABLE_CATALYST` | OFF | Enable Catalyst in-situ integration |
| `COEUS_ENABLE_DOXYGEN` | OFF | Generate documentation |
| `COEUS_ENABLE_COVERAGE` | OFF | Code coverage |

### Build Output
The primary build artifact is `libhermes_engine.so` (or `.dll`), which ADIOS2 dynamically loads as a plugin engine.

---

## 7. Test Suite

### Unit Tests (`test/unit/`)
- **JSONParser**: JSON configuration parsing
- **YAMLParser**: YAML configuration parsing
- **MPI**: MPI communication tests
- **SQLite**: Database operations and metadata storage
- **class_loader**: Dynamic library loading
- **GTest**: Google Test framework integration

### Integration Tests (`test/integration/`)
- **basic**: Single-variable put/get
- **basic_multi_variable**: Multi-variable operations
- **currentStep**: Step tracking
- **logging**: Metadata logging
- **metadata**: Metadata management
- **split_single_variable / split_multi_variable**: Data splitting

### Real Application Tests (`test/real_apps/`)
- **Gray-Scott**: Reaction-diffusion simulation with derived quantities (curl, add, hash)
- **hash_operator**: Hashing and comparison operators
- **io_comp**: I/O performance comparison (ADIOS vs NFS vs COEUS)
- **metadata_comp**: Metadata system comparison (Empress vs Hermes)
- **operator_comp**: Producer-consumer operator pipeline

### Jarvis Deployment Pipelines (`test/jarvis/`)
Pre-configured pipelines for full application deployments: WRF, LAMMPS, Gray-Scott, Incompact3D, OpenFOAM, ParaView.

---

## 8. Key Design Patterns

1. **Plugin Architecture**: COEUS is loaded dynamically by ADIOS2 via `EngineCreate()`/`EngineDestroy()` C functions, making it transparent to applications.

2. **Interface Abstraction**: `IHermes`/`ITag` interfaces decouple the engine from the specific storage backend (CTE), enabling testability and future backend swaps.

3. **Task-Based Metadata Management**: Metadata operations are dispatched as Chimaera tasks (`coeus_mdm::Mdm_insert`) for distributed, asynchronous execution.

4. **Step-Based Tag Organization**: Each simulation step + rank combination maps to a unique CTE tag (`step_{N}_rank{R}`), providing natural data partitioning.

5. **ADIOS2 Type Macros**: `ADIOS2_FOREACH_STDTYPE_1ARG` macros generate type-specific Put/Get overrides for all ADIOS2 standard types.

6. **Rank 0 Coordination**: Pool creation and certain operations are guarded by `mpi_rank == 0` with MPI barriers to prevent duplicate pool creation and ensure synchronization.

---

## 9. Source Code Statistics

| Category | Files | Lines (approx.) |
|----------|-------|-----------------|
| Core engine (`src/`, `include/coeus/`) | 5 | ~1,500 |
| Communication layer (`include/comms/`) | 6 | ~700 |
| Common utilities (`include/common/`) | 12 | ~1,000 |
| ChiMod tasks (`tasks/`) | ~16 | ~1,200 |
| Test code (`test/`) | ~60 | ~8,000 |
| Total project (excl. submodules/vendored) | ~107 | ~12,000 |

---

## 10. Current State and Notes

- The codebase is actively migrating from legacy Hermes to the IOWarp/CTE stack. Comments in `IHermes.h` note: "I/O is now handled by CTE (Context-Transfer-Engine), uses CTE Tags (replacing Hermes buckets)."
- The `src/CMakeLists.txt` contains extensive debug diagnostics (symbol checks via `nm`, include directory validation) from an active debugging session for linker issues with Chimaera symbols.
- Some metadata insertion calls in `DoPutSync_`/`DoPutDeferred_` are commented out (`//client.Mdm_insert(...)`) while derived variable metadata insertion remains active.
- Debug logging instrumentation (`#region agent log`) exists in several locations for runtime diagnostics.
- The `HermesEngine` name is retained for backward compatibility despite the underlying shift to CTE.
