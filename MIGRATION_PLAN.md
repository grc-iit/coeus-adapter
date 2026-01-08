# Migration Plan: Hermes Runtime to Context-Runtime Task Management

## Executive Summary

This document outlines the plan to migrate coeus-adapter from Hermes runtime task management to context-runtime (Chimaera) task management. The migration involves converting two task modules (`coeus_mdm` and `rankConsensus`) and updating the main engine to use the new runtime system.

## Current Architecture (Hermes Runtime)

### Key Components

1. **Task Libraries**: Use `hrun::TaskLib` base class for runtime implementation
2. **Task Clients**: Use `hrun::TaskLibClient` base class for client interface
3. **Task Registration**: Uses `HRUN_ADMIN->RegisterTaskLibRoot()` with domain-based routing
4. **Task Execution**: `Run()` method with `RunContext &rctx` parameter
5. **Task Types**: Inherit from `hrun::Admin::CreateTaskStateTask`, `hrun::Admin::DestroyTaskStateTask`
6. **Task Pointers**: Use `LPointer`, `TypedPushTask`, `PushTask`
7. **Domain Routing**: `DomainId::GetGlobal()`, `DomainId::GetLocal()`

### Current Task Modules

1. **coeus_mdm**: Metadata management module
   - Location: `tasks/coeus_mdm/`
   - Methods: `Construct`, `Destruct`, `Mdm_insert`
   - Uses SQLite for metadata storage

2. **rankConsensus**: Rank assignment module
   - Location: `tasks/rankConsensus/`
   - Methods: `Construct`, `Destruct`, `GetRank`
   - Provides rank assignment for MPI processes

### Integration Points

- `include/comms/Hermes.h`: Registers task libraries during Hermes connection
- `src/hermes_engine.cc`: Uses `hrun::coeus_mdm::Client` and `hrun::rankConsensus::Client`
- Task registration happens in `Hermes::connect()`

## Target Architecture (Context-Runtime/Chimaera)

### Key Components

1. **Containers**: Use `chi::Container` base class for runtime implementation
2. **Container Clients**: Use `chi::ContainerClient` base class for client interface
3. **Task Registration**: Uses module manager with `chimaera_mod.yaml` configuration
4. **Task Execution**: `Run()` method returning `chi::TaskResume` (coroutine-based)
5. **Task Types**: Inherit from `chi::Task` base class
6. **Task Pointers**: Use `hipc::FullPtr<Task>`, `chi::Future<Task>`
7. **Pool Routing**: `chi::PoolId`, `chi::PoolQuery::Local()`

### Key Differences

| Aspect | Hermes Runtime | Context-Runtime |
|--------|---------------|-----------------|
| Base Class (Runtime) | `hrun::TaskLib` | `chi::Container` |
| Base Class (Client) | `hrun::TaskLibClient` | `chi::ContainerClient` |
| Task Base | `hrun::Admin::CreateTaskStateTask` | `chi::Task` |
| Registration | `HRUN_ADMIN->RegisterTaskLibRoot()` | Module manager + YAML |
| Execution | `void Run(u32 method, Task* task, RunContext& rctx)` | `chi::TaskResume Run(u32 method, hipc::FullPtr<chi::Task> task, chi::RunContext& rctx)` |
| Task Pointers | `LPointer<Task>` | `hipc::FullPtr<Task>` |
| Async Operations | `LPointer<PushTask>` | `chi::Future<Task>` |
| Routing | `DomainId::GetGlobal/Local()` | `chi::PoolId`, `chi::PoolQuery` |
| Singletons | `HRUN_ADMIN`, `HRUN_CLIENT` | `CHI_ADMIN`, `CHI_IPC`, `CHI_CHIMAERA_MANAGER` |
| Initialization | `TRANSPARENT_HERMES()` | `chi::CHIMAERA_INIT()` |

## Migration Strategy

### Phase 1: Module Migration

#### 1.1 Migrate coeus_mdm Module

**Steps:**
1. Create `chimaera_mod.yaml` configuration file
2. Convert task definitions from Hermes to Chimaera format
3. Convert runtime implementation from `TaskLib` to `Container`
4. Convert client implementation from `TaskLibClient` to `ContainerClient`
5. Update CMakeLists.txt for Chimaera dependencies

**File Changes:**
- `tasks/coeus_mdm/chimaera_mod.yaml` (NEW)
- `tasks/coeus_mdm/include/coeus_mdm/coeus_mdm_tasks.h` (REWRITE)
- `tasks/coeus_mdm/include/coeus_mdm/coeus_mdm.h` (REWRITE)
- `tasks/coeus_mdm/src/coeus_mdm.cc` (REWRITE)
- `tasks/coeus_mdm/CMakeLists.txt` (UPDATE)

**Key Conversions:**
- `hrun::coeus_mdm::Server : public TaskLib` → `chimaera::coeus_mdm::Runtime : public chi::Container`
- `hrun::coeus_mdm::Client : public TaskLibClient` → `chimaera::coeus_mdm::Client : public chi::ContainerClient`
- `ConstructTask : public CreateTaskStateTask` → `CreateTask : public GetOrCreatePoolTask<CreateParams>`
- `Mdm_insertTask` → Inherit from `chi::Task` with proper serialization
- `void Mdm_insert(Mdm_insertTask *task, RunContext &rctx)` → `chi::TaskResume Mdm_insert(hipc::FullPtr<Mdm_insertTask> task, chi::RunContext &rctx)`

#### 1.2 Migrate rankConsensus Module

**Steps:**
1. Create `chimaera_mod.yaml` configuration file
2. Convert task definitions from Hermes to Chimaera format
3. Convert runtime implementation from `TaskLib` to `Container`
4. Convert client implementation from `TaskLibClient` to `ContainerClient`
5. Update CMakeLists.txt for Chimaera dependencies

**File Changes:**
- `tasks/rankConsensus/chimaera_mod.yaml` (NEW)
- `tasks/rankConsensus/include/rankConsensus/rankConsensus_tasks.h` (REWRITE)
- `tasks/rankConsensus/include/rankConsensus/rankConsensus.h` (REWRITE)
- `tasks/rankConsensus/src/rankConsensus.cc` (REWRITE)
- `tasks/rankConsensus/CMakeLists.txt` (UPDATE)

**Key Conversions:**
- `hrun::rankConsensus::Server : public TaskLib` → `chimaera::rankConsensus::Runtime : public chi::Container`
- `hrun::rankConsensus::Client : public TaskLibClient` → `chimaera::rankConsensus::Client : public chi::ContainerClient`
- `GetRankTask` → Inherit from `chi::Task` with proper serialization

### Phase 2: Engine Integration

#### 2.1 Update HermesEngine

**File:** `src/hermes_engine.cc`

**Changes:**
1. Replace Hermes runtime initialization with Chimaera initialization
2. Update client instantiation from `hrun::coeus_mdm::Client` to `chimaera::coeus_mdm::Client`
3. Update client instantiation from `hrun::rankConsensus::Client` to `chimaera::rankConsensus::Client`
4. Replace domain-based routing with pool-based routing
5. Update task submission from `Mdm_insertRoot()` to async `AsyncMdm_insert()` with `Future`

**Key Code Changes:**
```cpp
// OLD (Hermes Runtime)
TRANSPARENT_HERMES();
HRUN_ADMIN->RegisterTaskLibRoot(hrun::DomainId::GetGlobal(), "coeus_mdm");
HRUN_ADMIN->RegisterTaskLibRoot(hrun::DomainId::GetLocal(), "rankConsensus");
hrun::coeus_mdm::Client client;
client.CreateRoot(DomainId::GetGlobal(), "db_operation", db_file);
client.Mdm_insertRoot(DomainId::GetLocal(), db_op);

// NEW (Context-Runtime)
chi::CHIMAERA_INIT(chi::ChimaeraMode::kClient, true);
chimaera::admin::Client admin_client(chi::kAdminPoolId);
admin_client.Create(HSHM_MCTX, chi::PoolQuery::Local());
chimaera::coeus_mdm::Client client(chi::PoolId(8000, 0));
client.Create(HSHM_MCTX, chi::PoolQuery::Local(), create_params);
auto future = client.AsyncMdm_insert(HSHM_MCTX, chi::PoolQuery::Local(), db_op);
future.Wait();
```

#### 2.2 Update Hermes.h

**File:** `include/comms/Hermes.h`

**Changes:**
1. Remove `HRUN_ADMIN->RegisterTaskLibRoot()` calls
2. Add Chimaera initialization if needed
3. Update any task-related code to use Chimaera APIs

### Phase 3: CMake Configuration

#### 3.1 Root CMakeLists.txt

**File:** `CMakeLists.txt`

**Changes:**
1. Replace `find_package(Hermes CONFIG REQUIRED)` with `find_package(chimaera-core REQUIRED)`
2. Add Chimaera module dependencies:
   - `find_package(chimaera-admin REQUIRED)`
   - `find_package(chimaera-coeus_mdm REQUIRED)` (after module migration)
   - `find_package(chimaera-rankConsensus REQUIRED)` (after module migration)
3. Update include directories
4. Update link libraries

#### 3.2 Task Module CMakeLists.txt

**Files:**
- `tasks/coeus_mdm/CMakeLists.txt`
- `tasks/rankConsensus/CMakeLists.txt`

**Changes:**
1. Replace Hermes includes with Chimaera includes
2. Update library dependencies
3. Add Chimaera module export configuration
4. Update install rules

#### 3.3 Source CMakeLists.txt

**File:** `src/CMakeLists.txt`

**Changes:**
1. Add Chimaera client libraries to link dependencies
2. Update include paths

## Detailed Migration Steps

### Step 1: Create coeus_mdm Chimaera Module

#### 1.1 Create chimaera_mod.yaml
```yaml
module_name: coeus_mdm
namespace: chimaera
version: 1.0.0

# Inherited Methods
kCreate: 0
kDestroy: 1

# Custom Methods
kMdm_insert: 10
```

#### 1.2 Rewrite coeus_mdm_tasks.h

**Key Changes:**
- Replace `hrun::Admin::CreateTaskStateTask` with `chimaera::admin::GetOrCreatePoolTask<CreateParams>`
- Replace `hrun::Admin::DestroyTaskStateTask` with `chimaera::admin::DestroyTask`
- Convert `Mdm_insertTask` to inherit from `chi::Task`
- Add proper `SerializeIn`/`SerializeOut` methods
- Use `chi::priv::string` for string parameters
- Use `IN`/`OUT`/`INOUT` annotations

#### 1.3 Rewrite coeus_mdm.h (Client)

**Key Changes:**
- Change base class to `chi::ContainerClient`
- Replace `AsyncCreate()` to use `chimaera::admin::GetOrCreatePoolTask`
- Replace `Mdm_insertRoot()` with `AsyncMdm_insert()` returning `chi::Future`
- Use `chi::PoolId` and `chi::PoolQuery` instead of `DomainId`
- Use `CHI_IPC` instead of `HRUN_CLIENT`

#### 1.4 Rewrite coeus_mdm.cc (Runtime)

**Key Changes:**
- Change base class to `chi::Container`
- Replace `void Run(u32 method, Task* task, RunContext& rctx)` with `chi::TaskResume Run(u32 method, hipc::FullPtr<chi::Task> task, chi::RunContext& rctx)`
- Convert methods to coroutines (use `co_return` instead of `task->SetModuleComplete()`)
- Use `CHI_IPC` instead of `HRUN_CLIENT`
- Remove `HRUN_TASK_CC` macro, use module manager registration

### Step 2: Create rankConsensus Chimaera Module

Similar structure to coeus_mdm migration.

### Step 3: Update HermesEngine Integration

#### 3.1 Initialization Changes

**In `HermesEngine::Init_()`:**
```cpp
// OLD
TRANSPARENT_HERMES();
HRUN_ADMIN->RegisterTaskLibRoot(hrun::DomainId::GetGlobal(), "coeus_mdm");
HRUN_ADMIN->RegisterTaskLibRoot(hrun::DomainId::GetLocal(), "rankConsensus");

// NEW
chi::CHIMAERA_INIT(chi::ChimaeraMode::kClient, true);
// Module registration handled by module manager via chimaera_mod.yaml
```

#### 3.2 Client Creation Changes

**In `HermesEngine::Init_()`:**
```cpp
// OLD
hrun::coeus_mdm::Client client;
client.CreateRoot(DomainId::GetGlobal(), "db_operation", db_file);

hrun::rankConsensus::Client rank_consensus;
rank_consensus.CreateRoot(DomainId::GetLocal(), "rankConsensus");

// NEW
// First create admin client and pool
chimaera::admin::Client admin_client(chi::kAdminPoolId);
admin_client.Create(HSHM_MCTX, chi::PoolQuery::Local());

// Create coeus_mdm pool
chimaera::coeus_mdm::Client client(chi::PoolId(8000, 0));
chimaera::coeus_mdm::CreateParams mdm_params;
mdm_params.db_path_ = db_file;
client.Create(HSHM_MCTX, chi::PoolQuery::Local(), mdm_params);

// Create rankConsensus pool
chimaera::rankConsensus::Client rank_consensus(chi::PoolId(8001, 0));
rank_consensus.Create(HSHM_MCTX, chi::PoolQuery::Local());
```

#### 3.3 Task Submission Changes

**In `HermesEngine::DoPutSync_()` and `DoPutDeferred_()`:**
```cpp
// OLD
DbOperation db_op(currentStep, rank, std::move(vm), name, std::move(blobInfo));
client.Mdm_insertRoot(DomainId::GetLocal(), db_op);

// NEW
DbOperation db_op(currentStep, rank, std::move(vm), name, std::move(blobInfo));
auto future = client.AsyncMdm_insert(HSHM_MCTX, chi::PoolQuery::Local(), db_op);
future.Wait();  // Or use co_await in coroutine context
```

**In `HermesEngine::Init_()` for rank consensus:**
```cpp
// OLD
rank_consensus.CreateRoot(DomainId::GetLocal(), "rankConsensus");
rank = rank_consensus.GetRankRoot(DomainId::GetLocal());

// NEW
rank_consensus.Create(HSHM_MCTX, chi::PoolQuery::Local());
auto future = rank_consensus.AsyncGetRank(HSHM_MCTX, chi::PoolQuery::Local());
future.Wait();
rank = future->rank_;
```

### Step 4: CMake Updates

#### 4.1 Root CMakeLists.txt

```cmake
# Replace Hermes find_package
# OLD: find_package(Hermes CONFIG REQUIRED)

# NEW: Find Chimaera packages
find_package(chimaera-core REQUIRED)
find_package(chimaera-admin REQUIRED)
find_package(chimaera-coeus_mdm REQUIRED)
find_package(chimaera-rankConsensus REQUIRED)

# Update include directories
include_directories(${chimaera-core_INCLUDE_DIRS})
```

#### 4.2 Task Module CMakeLists.txt

```cmake
# OLD structure
include_directories(
    include
    ${Hermes_DIR}/include
)

# NEW structure (for coeus_mdm example)
find_package(chimaera-core REQUIRED)
find_package(chimaera-admin REQUIRED)

add_library(chimaera_coeus_mdm SHARED
    src/coeus_mdm.cc
    src/autogen/coeus_mdm_lib_exec.cc
)

target_include_directories(chimaera_coeus_mdm PUBLIC
    include
    ${chimaera-core_INCLUDE_DIRS}
    ${chimaera-admin_INCLUDE_DIRS}
)

target_link_libraries(chimaera_coeus_mdm
    chimaera::cxx
    chimaera::admin_client
    SQLite3::SQLite3
)
```

## Testing Strategy

### Unit Tests
1. Test each migrated module independently
2. Test task serialization/deserialization
3. Test client-server communication
4. Test pool creation and destruction

### Integration Tests
1. Test HermesEngine initialization with Chimaera
2. Test metadata insertion workflow
3. Test rank consensus workflow
4. Test end-to-end ADIOS2 operations

### Regression Tests
1. Run existing test suite
2. Compare behavior with previous Hermes runtime version
3. Performance benchmarking

## Risk Assessment

### High Risk Areas
1. **Task Serialization**: Different serialization mechanisms between systems
2. **Async Operations**: Different async patterns (LPointer vs Future)
3. **Initialization Order**: Chimaera requires specific initialization sequence
4. **Pool Management**: New pool-based routing vs domain-based routing

### Mitigation Strategies
1. Create wrapper/adapter layer if needed for gradual migration
2. Maintain backward compatibility during transition
3. Comprehensive testing at each migration step
4. Keep old code commented for reference during migration

## Rollback Plan

1. Keep Hermes runtime code in version control
2. Use feature flags to switch between runtimes
3. Maintain separate build configurations
4. Document rollback procedure

## Timeline Estimate

- **Phase 1 (Module Migration)**: 2-3 weeks
  - coeus_mdm: 1 week
  - rankConsensus: 1 week
  - Testing: 1 week

- **Phase 2 (Engine Integration)**: 1-2 weeks
  - Code changes: 3-5 days
  - Integration testing: 3-5 days

- **Phase 3 (CMake Updates)**: 3-5 days

- **Total**: 4-6 weeks

## Dependencies

1. Context-runtime must be built and installed
2. Chimaera module development tools available
3. Access to context-runtime documentation
4. Test infrastructure ready

## Success Criteria

1. All existing functionality works with context-runtime
2. Performance is equal or better than Hermes runtime
3. Code is cleaner and more maintainable
4. All tests pass
5. Documentation updated

## Next Steps

1. Review and approve this migration plan
2. Set up development environment with context-runtime
3. Create feature branch for migration
4. Begin Phase 1: Module migration
5. Regular progress reviews

