# Migration Summary: Hermes Runtime to Context-Runtime

## Overview
Successfully migrated coeus-adapter from Hermes runtime task management to Context-Runtime (Chimaera) task management system.

## Completed Tasks

### ✅ Step 1: Migrated coeus_mdm Module
- Created Chimaera module structure with proper namespace (`chimaera::coeus_mdm`)
- Converted from `hrun::TaskLib` to `chi::Container`
- Converted from `hrun::TaskLibClient` to `chi::ContainerClient`
- Updated task definitions to use `chi::Task` base class
- Implemented proper serialization with `SerializeIn`/`SerializeOut`
- Created `chimaera_mod.yaml` configuration file
- Generated autogen files for virtual method implementations

**Files Created:**
- `tasks/coeus_mdm/chimaera_mod.yaml`
- `tasks/coeus_mdm/include/chimaera/coeus_mdm/coeus_mdm_tasks.h`
- `tasks/coeus_mdm/include/chimaera/coeus_mdm/coeus_mdm_client.h`
- `tasks/coeus_mdm/include/chimaera/coeus_mdm/coeus_mdm_runtime.h`
- `tasks/coeus_mdm/include/chimaera/coeus_mdm/autogen/coeus_mdm_methods.h`
- `tasks/coeus_mdm/src/coeus_mdm_runtime.cc`
- `tasks/coeus_mdm/src/coeus_mdm_client.cc`
- `tasks/coeus_mdm/src/autogen/coeus_mdm_lib_exec.cc`

### ✅ Step 2: Migrated rankConsensus Module
- Created Chimaera module structure with proper namespace (`chimaera::rankConsensus`)
- Converted from `hrun::TaskLib` to `chi::Container`
- Converted from `hrun::TaskLibClient` to `chi::ContainerClient`
- Updated `GetRankTask` to use `chi::Task` base class
- Implemented thread-safe rank assignment using `std::atomic<chi::u32>`
- Created `chimaera_mod.yaml` configuration file
- Generated autogen files for virtual method implementations

**Files Created:**
- `tasks/rankConsensus/chimaera_mod.yaml`
- `tasks/rankConsensus/include/chimaera/rankConsensus/rankConsensus_tasks.h`
- `tasks/rankConsensus/include/chimaera/rankConsensus/rankConsensus_client.h`
- `tasks/rankConsensus/include/chimaera/rankConsensus/rankConsensus_runtime.h`
- `tasks/rankConsensus/include/chimaera/rankConsensus/autogen/rankConsensus_methods.h`
- `tasks/rankConsensus/src/rankConsensus_runtime.cc`
- `tasks/rankConsensus/src/rankConsensus_client.cc`
- `tasks/rankConsensus/src/autogen/rankConsensus_lib_exec.cc`

### ✅ Step 3: Updated HermesEngine
- Replaced Hermes runtime initialization with Chimaera initialization
- Updated client declarations from `hrun::coeus_mdm::Client` to `chimaera::coeus_mdm::Client`
- Updated client declarations from `hrun::rankConsensus::Client` to `chimaera::rankConsensus::Client`
- Replaced `TRANSPARENT_HERMES()` with `chi::CHIMAERA_INIT(chi::ChimaeraMode::kClient, true)`
- Replaced domain-based routing with pool-based routing
- Updated task submission from `Mdm_insertRoot()` to `Mdm_insert()`
- Updated rank consensus from `GetRankRoot()` to `GetRank()`
- Added pool ID management for both modules

**Key Changes in `src/hermes_engine.cc`:**
```cpp
// OLD:
TRANSPARENT_HERMES();
HRUN_ADMIN->RegisterTaskLibRoot(...);
rank_consensus.CreateRoot(DomainId::GetLocal(), "rankConsensus");
rank = rank_consensus.GetRankRoot(DomainId::GetLocal());
client.CreateRoot(DomainId::GetGlobal(), "db_operation", db_file);
client.Mdm_insertRoot(DomainId::GetLocal(), db_op);

// NEW:
chi::CHIMAERA_INIT(chi::ChimaeraMode::kClient, true);
chimaera::admin::Client admin_client(chi::kAdminPoolId);
admin_client.Create(chi::PoolQuery::Local(), "admin", chi::kAdminPoolId);
rank_consensus.Create(chi::PoolQuery::Local(), "rankConsensus", rankConsensus_pool_id_);
rank = rank_consensus.GetRank(chi::PoolQuery::Local());
client.Create(chi::PoolQuery::Local(), "db_operation", coeus_mdm_pool_id_, db_file);
client.Mdm_insert(chi::PoolQuery::Local(), db_op);
```

**Key Changes in `include/coeus/HermesEngine.h`:**
- Updated includes to use Chimaera headers
- Changed client types from `hrun::*` to `chimaera::*`
- Added pool ID members: `coeus_mdm_pool_id_` and `rankConsensus_pool_id_`

**Key Changes in `include/comms/Hermes.h`:**
- Removed `HRUN_ADMIN->RegisterTaskLibRoot()` calls
- Updated comments to clarify Hermes is only for storage backend, not task management

### ✅ Step 4: Updated CMake Configuration

**Root `CMakeLists.txt`:**
- Added `find_package(chimaera-core REQUIRED)`
- Added `find_package(chimaera-admin REQUIRED)`
- Kept Hermes for storage backend (separate from task management)

**`tasks/coeus_mdm/CMakeLists.txt`:**
- Complete rewrite for Chimaera module structure
- Creates separate client and runtime libraries
- Links against Chimaera libraries instead of Hermes runtime

**`tasks/rankConsensus/CMakeLists.txt`:**
- Complete rewrite for Chimaera module structure
- Creates separate client and runtime libraries
- Links against Chimaera libraries instead of Hermes runtime

**`src/CMakeLists.txt`:**
- Updated to link against Chimaera libraries
- Added dependencies on new Chimaera module libraries

## Architecture Changes

### Task Management System
- **Before**: Hermes Runtime (HRUN) with domain-based routing
- **After**: Context-Runtime (Chimaera) with pool-based routing

### Module Structure
- **Before**: `hrun::TaskLib` / `hrun::TaskLibClient` with manual registration
- **After**: `chi::Container` / `chi::ContainerClient` with YAML-based module registration

### Task Execution
- **Before**: `void Run(u32 method, Task* task, RunContext& rctx)`
- **After**: `chi::TaskResume Run(u32 method, hipc::FullPtr<chi::Task> task, chi::RunContext& rctx)` (coroutine-based)

### Async Operations
- **Before**: `LPointer<PushTask>` with fire-and-forget semantics
- **After**: `chi::Future<Task>` with proper async/await support

## Pool IDs Used
- **Admin Pool**: `chi::kAdminPoolId` (7000, 0) - system-defined
- **coeus_mdm Pool**: `chi::PoolId(8000, 0)` - user-defined
- **rankConsensus Pool**: `chi::PoolId(8001, 0)` - user-defined

## Next Steps

1. **Testing**: Run unit tests and integration tests to verify functionality
2. **Runtime Setup**: Ensure Chimaera runtime is running (via `chimaera_start_runtime` or embedded mode)
3. **Configuration**: Verify `chimaera_mod.yaml` files are in the correct location for module manager
4. **Performance**: Benchmark and compare with previous Hermes runtime version

## Notes

- Hermes storage backend is still used for actual data storage (buckets, blobs)
- Only task management has been migrated to Chimaera
- The migration maintains backward compatibility for ADIOS2 interface
- All existing functionality should work the same from the user's perspective

## Files Modified

### Headers
- `include/coeus/HermesEngine.h`
- `include/comms/Hermes.h`

### Sources
- `src/hermes_engine.cc`

### CMake
- `CMakeLists.txt` (root)
- `tasks/coeus_mdm/CMakeLists.txt`
- `tasks/rankConsensus/CMakeLists.txt`
- `src/CMakeLists.txt`

## Migration Status: ✅ COMPLETE

All code changes have been completed. The system is ready for testing and validation.

