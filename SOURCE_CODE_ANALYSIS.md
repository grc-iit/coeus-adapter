# COEUS-Adapter Source Code Analysis

> Focus: what COEUS-Adapter is, and **how it depends on clio-core** (the IOWarp
> core: Chimaera runtime + Context-Transfer-Engine + transport primitives).

## 1. Project Overview

**COEUS-Adapter** is an **ADIOS2 plugin engine** (`libhermes_engine.so`) that
bridges ADIOS2 applications to the IOWarp / clio storage stack. Applications keep
using the ordinary ADIOS2 API; COEUS intercepts I/O through
`adios2::plugin::PluginEngineInterface` and redirects it into clio-core's
**Context-Transfer-Engine (CTE)** for multi-tiered buffering. On top of that it
adds in-situ derived-variable computation (curl, Q-criterion, hash), SQLite-backed
metadata management, and optional in-situ visualization via Catalyst/Fides.

- **Language**: C++17, with C++20 + `-fcoroutines` required for any target that
  touches Chimaera task bodies (`hermes_engine` and both ChiMods)
- **License**: BSD 3-Clause (Illinois Institute of Technology)
- **Build system**: CMake 3.10+
- **State**: mid-migration - Hermes I/O has been fully replaced by CTE, but the
  Hermes-era names (`HermesEngine`, `hermes_engine`, `IHermes`) are retained

The single shippable artifact is `hermes_engine`, built from `hermes_engine.cc`
+ `CTEHermes.cc` + `CTETagClient.cc`, plus two local Chimaera modules
(`coeus_mdm`, `rankConsensus`) that run inside the clio-core runtime.

---

## 2. Dependency on clio-core (the core of this analysis)

COEUS-Adapter does **not** vendor the IOWarp core. The entire dependency flows
through **one CMake package** - `find_package(iowarp-core REQUIRED)` - which is
built and installed from the separate **clio-core** repository. That package
bundles three layers, all of which COEUS consumes:

| clio-core layer | clio-core dir | What COEUS uses it for | CMake targets linked |
|---|---|---|---|
| **Chimaera** - task-execution runtime | `context-runtime/` | Task scheduling, pools, shared-memory IPC (`CHI_IPC`, `hipc::FullPtr`, allocators), the ChiMod programming model, C++20 coroutine tasks | `chimaera::cxx`, `chimaera::admin_client`, `chimaera::bdev_client` |
| **CTE** - Context-Transfer-Engine, tiered blob store | `context-transfer-engine/core/` | The actual I/O: tags (≈ buckets), `PutBlob`/`GetBlob`/`GetBlobSize`, scoring-based reorganize (demote/prefetch) | `wrp_cte::core_client` |
| **CTP / HermesShm** - transport primitives | `context-transport-primitives/` | Low-level shared-memory types (`hermes::Blob`, `hipc::ShmPtr`), pulled in transitively | (via the above) |

> **Note:** the `context-transfer-engine/`, `context-runtime/`,
> `context-transport-primitives/`, `context-assimilation-engine/`,
> `context-exploration-engine/` directories live in **clio-core**, *not* inside
> coeus-adapter. COEUS-Adapter's own `.gitmodules` declares only
> `CI/jarvis-util` and `CI/jarvis-cd`.

### 2.1 Build coupling

- clio-core must be **built and installed first**, such that
  `iowarp-core-config.cmake` is discoverable on `CMAKE_PREFIX_PATH`. (No installed
  `iowarp-core-config.cmake` was found on this system, so the package is not yet
  present - clio-core's `install.sh`/Spack flow needs to run before COEUS can
  configure.)
- Both projects force **C++20 + `-fcoroutines`** because Chimaera task bodies are
  C++20 coroutines (`TaskResume`). See `set_target_properties(... CXX_STANDARD 20)`
  in `src/CMakeLists.txt` and each `tasks/*/CMakeLists.txt`.
- COEUS defines its **own ChiMods** (`tasks/coeus_mdm`, `tasks/rankConsensus`)
  against the clio-core ChiMod API; they compile against clio-core headers and are
  loaded by the Chimaera runtime at deploy time.

### 2.2 Rebrand / naming compatibility (important)

clio-core has been **rebranded** (`clio-core/rebranding.md`):
`chimaera → clio_runtime`, `hermes_shm`/`HSHM`/`hshm:: → clio_ctp`/`CTP_`/`ctp::`,
and the `wrp_*` packages now have `clio_*` equivalents (e.g.
`clio_cte/core/core_client.h` sits alongside `wrp_cte/core/core_client.h`).

**COEUS-Adapter still uses every legacy name** - `wrp_cte::core`, `chimaera::`,
`CHI_IPC`, `hipc::`, `HSHM_MALLOC`, `find_package(iowarp-core)`. This compiles only
because clio-core keeps a complete backward-compatibility surface (forwarder
headers, `#define CLIO_X CHI_X`, `namespace hshm = ctp`). Consequence: COEUS is
pinned to clio-core's *deprecated-name compat shims* rather than the canonical new
API. A future cleanup is to migrate to the `clio_*`/`ctp::` identifiers.

### 2.3 Concrete integration points

- **`include/comms/CTEHermes.{h,cc}`** - `CTEHermes` multiply-inherits `IHermes`
  + `wrp_cte::core::Client`, so it *is* a CTE client.
  - `connect()` calls `wrp_cte::core::WRP_CTE_CLIENT_INIT("", chi::PoolQuery::Local())`
    and attaches to a pre-deployed CTE core pool (`kCtePoolId = 512.0`, started by
    Jarvis). The Chimaera runtime must already be running.
  - `Put()` allocates shared memory via `CHI_IPC->AllocateBuffer`, `memcpy`s the
    payload, and issues `AsyncPutBlob(...).Wait()` with a placement score of 0.7.
  - `Demote()`/`Prefetch()` map to `AsyncReorganizeBlob` with scores 0.3 / 0.95.
- **`include/comms/CTETagClient.{h,cc}`** - per-tag wrapper over the same CTE
  client: `AsyncGetOrCreateTag`, `AsyncGetBlob`, `AsyncGetBlobSize`,
  `AsyncGetContainedBlobs`.
- **`tasks/coeus_mdm` & `tasks/rankConsensus`** - COEUS's own ChiMods, written
  against the current clio-core API (`chi::Task`, `chi::ContainerClient`,
  `chimaera::admin::GetOrCreatePoolTask<CreateParams>`, `Method::k...`).
- **`include/coeus/HermesEngine.h`** includes `<chimaera/chimaera.h>`,
  `<chimaera/admin/admin_client.h>`, `<wrp_cte/core/core_client.h>` and holds
  `chi::PoolId`, `chimaera::coeus_mdm::Client`, `chimaera::rankConsensus::Client`
  members.

---

## 3. Architecture

```
+----------------------------+
|   Scientific Application   |
|  (WRF, LAMMPS, Gray-Scott, |
|   Incompact3D, OpenFOAM)   |
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
+---+--------+--------+-------+       +---------------------+
    |        |        |
    v        v        v
+---------+ +------+ +----------+
|CTEHermes| |SQLite| | ChiMods  |
|(IHermes)| |(meta)| | (coeus_  |
+----+----+ +------+ |  mdm,    |
     |               | rankCons)|
     |               +----+-----+
     |   ============= clio-core ============= |
     v                    v
+-------------------+ +-------------------+
| CTE  (wrp_cte::    | | Chimaera Runtime  |
|  core::Client)     | | (chimaera::cxx)   |
| tags + blobs       | | pools, IPC, mods  |
+---------+----------+ +---------+---------+
          \                     /
           v                   v
     +-------------------------------+
     | CTP / HermesShm (hipc::, shm) |
     +-------------------------------+
```

### 3.1 Core data flow

1. **Write**: `adios2::Put()` → `HermesEngine::DoPutSync_/DoPutDeferred_` →
   `CTEHermes::Put()` → CTE blob (shared-memory buffer + `AsyncPutBlob`). Metadata
   is generated and dispatched via the `coeus_mdm` ChiMod and/or SQLite.
2. **Read**: `adios2::Get()` → `HermesEngine::DoGetSync_/DoGetDeferred_` →
   `CTETagClient::Get()` → `AsyncGetBlob` → `memcpy` into the user buffer.
3. **Derived variables**: at `EndStep()`, `ComputeDerivedVariables()` reads input
   blobs from CTE, applies ADIOS2 `VariableDerived` expressions (curl,
   Q-criterion, hash, …), and writes results back via `PutDerived()`.

---

## 4. Directory Structure (coeus-adapter only)

```
coeus-adapter/
├── src/
│   ├── hermes_engine.cc          # Main ADIOS2 plugin engine (~1040 lines)
│   └── CMakeLists.txt            # Builds hermes_engine; links iowarp-core targets
├── include/
│   ├── coeus/                    # Engine headers
│   │   ├── HermesEngine.h        # Main engine class definition
│   │   ├── Container.h           # Derived-variable container
│   │   ├── ContainerManager.h    # Container orchestration
│   │   └── MetadataSerializer.h
│   ├── comms/                    # Storage backend integration (the clio-core seam)
│   │   ├── interfaces/
│   │   │   ├── IHermes.h         # Abstract I/O interface
│   │   │   ├── ITag.h            # Abstract tag/blob interface
│   │   │   └── IMPI.h
│   │   ├── CTEHermes.h/.cc       # IHermes impl over wrp_cte::core::Client
│   │   ├── CTETagClient.h/.cc    # ITag impl over the CTE client
│   │   └── MPI.h
│   └── common/                   # Shared utilities
│       ├── SQlite.h, DbOperation.h, DbWorker.h
│       ├── MetadataStructs.h, VariableMetadata.h
│       ├── YAMLParser.h, JSONParser.h
│       ├── ErrorCodes.h, ErrorDefinition.h, Tracer.h
│       ├── ThreadPool.h, globalVariable.h
│       ├── CatalystHelper.h      # Catalyst in-situ viz integration
│       └── ClassLoader.h
├── tasks/                        # COEUS's own Chimaera ChiMods (run in clio-core runtime)
│   ├── coeus_mdm/                # Metadata-management ChiMod
│   │   ├── include/chimaera/coeus_mdm/   # current API (client/runtime/tasks)
│   │   ├── include/coeus_mdm/            # STALE old hrun-era headers (see §9)
│   │   └── src/
│   └── rankConsensus/            # Distributed rank-assignment ChiMod
├── test/                         # unit/, integration/, real_apps/, jarvis/
├── external_libraries/           # Vendored: spdlog, cereal, rapidjson
├── config/cte_config.yaml        # CTE storage-target / DPE configuration
├── CI/                           # jarvis-util, jarvis-cd submodules; Spack; Docker
└── .github/                      # GitHub Actions CI
```

---

## 5. Core Components

### 5.1 HermesEngine (`src/hermes_engine.cc`, `include/coeus/HermesEngine.h`)

The central class, inheriting `adios2::plugin::PluginEngineInterface` - what
ADIOS2 dynamically loads as a plugin engine.

**Responsibilities:**
- **Init**: connects to the Chimaera runtime (client mode), initializes CTE via
  `CTEHermes::connect()`, creates the `rankConsensus` and `coeus_mdm` pools, parses
  YAML operator/variable configs.
- **Steps**: `BeginStep()` creates a CTE tag per step/rank (`step_{N}_rank{R}`);
  `EndStep()` computes derived variables and cleans up.
- **Put/Get**: `DoPutSync_`/`DoPutDeferred_` and `DoGetSync_`/`DoGetDeferred_`,
  generated for every ADIOS2 standard type via `ADIOS2_FOREACH_STDTYPE_1ARG`.
- **Derived variables**: `ComputeDerivedVariables()` / `PutDerived()`.
- **Tiering**: `Promote()`/`Demote()` map to CTE reorganize-by-score.
- **Catalyst**: optional in-situ viz (Inline single-node, SST multi-node).

**Configuration parameters** (via ADIOS2 XML): `OPFile`, `VarFile`, `ppn`,
`limit`, `lookahead`, `db_file`, `Script`, `DataModel`, `CatalystStream`.

### 5.2 Communication layer (`include/comms/`)

- **`IHermes`** - abstract I/O interface: `connect`, `GetTag`, `Put`, `Demote`,
  `Prefetch`, and a `tag` pointer.
- **`ITag`** - abstract per-tag blob interface: `Put`, `Get`,
  `GetContainedBlobNames`, `GetBlobSize`.
- **`CTEHermes`** / **`CTETagClient`** - the concrete CTE implementations
  described in §2.3. This interface seam is the abstraction over clio-core; in
  principle it allows swapping the backend, though CTE is the only impl today.

### 5.3 Chimaera ChiMods (`tasks/`)

- **`coeus_mdm`** (metadata manager): client API `AsyncCreate(query, name,
  pool_id, db_path)` and `Mdm_insert(query, db_op)`; runtime handles `DbOperation`
  objects backed by SQLite. `CreateTask` is a
  `chimaera::admin::GetOrCreatePoolTask<CreateParams>`.
- **`rankConsensus`** (rank assignment): coordinated unique-rank assignment across
  MPI processes via the runtime.

### 5.4 Metadata management (`include/common/`)

`SQLiteWrapper` (`SQlite.h`) with tables for apps, blob locations, variable
metadata, and derived-target semantics; `DbOperation` encapsulates a metadata op
(step, rank, variable metadata, blob info) and is serialized with cereal for
transport into the `coeus_mdm` ChiMod.

### 5.5 Catalyst/Fides in-situ visualization (optional)

Guarded by `COEUS_HAVE_CATALYST`. Inline mode (single-node, in-process) or SST
mode (multi-node, external Catalyst reader). Functions `CatalystConfig()`,
`CatalystInit()`, `CatalystExecute()`.

---

## 6. Build System and Dependencies

### External dependencies
| Dependency | Purpose | Required |
|---|---|---|
| `iowarp-core` | **clio-core** unified package (Chimaera + CTE + CTP/HermesShm) | Yes |
| `ADIOS2` | I/O framework, plugin host | Yes |
| `MPI` (C + CXX) | Distributed communication | Yes |
| `yaml-cpp` | YAML config parsing | Yes |
| `OpenMP` (C + CXX) | Parallel computation | Yes |
| `SQLite3` | Metadata storage | Yes |
| `GTest` | Unit testing | Yes |
| `Catalyst` | In-situ visualization | No |

### Vendored libraries (`external_libraries/`)
`spdlog` (logging), `cereal` (binary serialization for metadata), `rapidjson`.

### Build options
| Option | Default | Description |
|---|---|---|
| `meta_enabled` | OFF | Enable metadata collection (defines `Meta_enabled`) |
| `debug_mode` | OFF | Enable debug mode |
| `COEUS_ENABLE_CATALYST` | OFF | Enable Catalyst in-situ integration |
| `COEUS_ENABLE_JARVIS` | ON | Enable Jarvis deployment integration |
| `COEUS_ENABLE_DOXYGEN` | OFF | Generate documentation |
| `COEUS_ENABLE_COVERAGE` | OFF | Code coverage |

### Build output
Primary artifact: `libhermes_engine.so`, dynamically loaded by ADIOS2 as a plugin
engine.

---

## 7. Test Suite

- **Unit (`test/unit/`)**: JSONParser, YAMLParser, MPI, SQLite, class_loader, GTest.
- **Integration (`test/integration/`)**: basic / multi-variable put-get, step
  tracking, logging, metadata, data splitting.
- **Real apps (`test/real_apps/`)**: Gray-Scott (curl/add/hash), hash_operator,
  io_comp, metadata_comp, operator_comp.
- **Jarvis pipelines (`test/jarvis/`)**: WRF, LAMMPS, Gray-Scott, Incompact3D,
  OpenFOAM, ParaView - these also stand up the clio-core runtime + CTE core pool
  that the engine attaches to.

---

## 8. Key Design Patterns

1. **Plugin architecture** - loaded by ADIOS2 via `EngineCreate()`/`EngineDestroy()`,
   transparent to applications.
2. **Backend abstraction** - `IHermes`/`ITag` decouple the engine from clio-core's
   CTE, the single integration seam.
3. **Task-based metadata** - metadata ops dispatched as Chimaera tasks
   (`coeus_mdm::Mdm_insert`) for asynchronous distributed execution.
4. **Step-based tag organization** - each `(step, rank)` maps to a CTE tag
   `step_{N}_rank{R}`.
5. **Type-macro generation** - `ADIOS2_FOREACH_STDTYPE_1ARG` generates the
   per-type Put/Get overrides.
6. **Rank-0 coordination** - pool creation guarded by `rank == 0` + MPI barriers.

---

## 9. Current State and Migration Debt

1. **Hermes → CTE migration is naming-incomplete.** I/O is 100% CTE, but the class
   is still `HermesEngine`, the library is `hermes_engine`, and the interface is
   `IHermes`. `IHermes.h` notes: "I/O is now handled by CTE … CTE Tags (replacing
   Hermes buckets)."
2. **Two parallel ChiMod task definitions coexist** for `coeus_mdm`:
   - `tasks/coeus_mdm/include/coeus_mdm/coeus_mdm_tasks.h` - **stale** old
     `hrun::`-era API (`CreateTaskStateTask`, `HSHM_MAKE_AR`, `DomainId`).
   - `tasks/coeus_mdm/include/chimaera/coeus_mdm/coeus_mdm_tasks.h` - **current**
     API (`chi::Task`, `GetOrCreatePoolTask<CreateParams>`, `chi::PoolQuery`); this
     is what `HermesEngine.h` actually includes. The old tree should be deleted.
3. **Committed build-debug cruft** in `src/CMakeLists.txt`: a large
   `#region agent log` block runs `nm`/`file(READ)` diagnostics on `chimaera::cxx`
   at configure time (writing `.cursor/debug.log`), left over from chasing missing
   `PoolQuery`/`AwakenWorker` symbols and conflicting `chimaera/types.h` -
   classic ABI/allocator-mismatch symptoms from the rebrand. Should be removed.
4. **Hardcoded paths / overridden flags**: `CTEHermes::connect()` sets
   `const bool use_pre_deployed = true;`, ignoring the `CTE_PRE_DEPLOYED` env var it
   reads; the create-path registers `/mnt/common/hxu40/cte_storage`;
   `config/cte_config.yaml` points at `/tmp/cte_primary` and `/tmp/cte_cache`.
5. **Legacy compat-name reliance** - COEUS uses clio-core's deprecated
   `chimaera`/`wrp_cte`/`hshm` aliases rather than the new `clio_*`/`ctp::` API
   (see §2.2).
6. **README lag** - README still references `spack load hermes`, which conflicts
   with the CTE-only reality.
