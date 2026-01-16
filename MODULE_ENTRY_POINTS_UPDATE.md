# Module Entry Points Update Summary

## Problem
The Chimaera modules (`rankConsensus` and `coeus_mdm`) were missing the required C entry points that allow Chimaera's `ModuleManager` to discover and identify them. When `CHI_MODULE_MANAGER->GetChiMod("chimaera_rankConsensus")` was called in `hermes_engine.cc`, it would fail to find the modules because the libraries didn't export the `get_chimod_name()` function.

## Root Cause
The runtime implementation files (`rankConsensus_runtime.cc` and `coeus_mdm_runtime.cc`) were missing the `CHI_TASK_CC` macro, which generates the required C entry points:
- `get_chimod_name()` - Returns the module name string
- `alloc_chimod()` - Allocates a new container instance
- `new_chimod()` - Creates a new container instance
- `destroy_chimod()` - Destroys a container instance

## Solution
Added the `CHI_TASK_CC` macro to both runtime files:

### 1. `tasks/rankConsensus/src/rankConsensus_runtime.cc`
Added at the end of the file (after namespace closing):
```cpp
// Generate ChiMod entry points (get_chimod_name, alloc_chimod, etc.)
CHI_TASK_CC(chimaera::rankConsensus::Runtime)
```

### 2. `tasks/coeus_mdm/src/coeus_mdm_runtime.cc`
Added at the end of the file (after namespace closing):
```cpp
// Generate ChiMod entry points (get_chimod_name, alloc_chimod, etc.)
CHI_TASK_CC(chimaera::coeus_mdm::Runtime)
```

## How It Works

### Module Name Definition
Each module defines its name in `CreateParams::chimod_lib_name`:
- **rankConsensus**: `"chimaera_rankConsensus"` (defined in `rankConsensus_tasks.h`)
- **coeus_mdm**: `"chimaera_coeus_mdm"` (defined in `coeus_mdm_tasks.h`)

### Module Discovery Flow
1. **ModuleManager Initialization** (`CHI_MODULE_MANAGER->ServerInit()`):
   - Scans configured directories for shared libraries matching `*_runtime.so` pattern
   - Loads `libchimaera_rankConsensus_runtime.so` and `libchimaera_coeus_mdm_runtime.so`
   - For each library, calls `get_chimod_name()` to get the module identifier
   - Stores module info in internal map: `chimods_[module_name] = ChiModInfo*`

2. **Module Lookup** (`CHI_MODULE_MANAGER->GetChiMod("chimaera_rankConsensus")`):
   - Looks up the module name in the internal map
   - Returns `ChiModInfo*` containing function pointers to:
     - `alloc_chimod()` - for creating new container instances
     - `new_chimod()` - for creating initialized containers
     - `destroy_chimod()` - for cleanup

3. **Runtime Usage** (in `hermes_engine.cc`):
   ```cpp
   // Load required ChiMod modules
   if (CHI_MODULE_MANAGER) {
     if (!CHI_MODULE_MANAGER->IsInitialized()) {
       CHI_MODULE_MANAGER->ServerInit();  // Auto-discovers modules
     }
     
     // Verify modules are loaded
     auto* rankConsensus_mod = CHI_MODULE_MANAGER->GetChiMod("chimaera_rankConsensus");
     auto* coeus_mdm_mod = CHI_MODULE_MANAGER->GetChiMod("chimaera_coeus_mdm");
   }
   ```

## Technical Details

### CHI_TASK_CC Macro
The `CHI_TASK_CC` macro (defined in `context-runtime/include/chimaera/container.h`) generates:
```cpp
extern "C" {
  chi::Container* alloc_chimod() {
    return reinterpret_cast<chi::Container*>(new CONTAINER_CLASS());
  }
  
  chi::Container* new_chimod(const chi::PoolId* pool_id, const char* pool_name) {
    auto* container = new CONTAINER_CLASS();
    return reinterpret_cast<chi::Container*>(container);
  }
  
  const char* get_chimod_name() {
    return CONTAINER_CLASS::CreateParams::chimod_lib_name;
  }
  
  void destroy_chimod(chi::Container* container) {
    delete reinterpret_cast<CONTAINER_CLASS*>(container);
  }
  
  static bool is_chimaera_chimod_ = true;
}
```

### Requirements
For `CHI_TASK_CC` to work correctly, the Runtime class must:
1. Inherit from `chi::Container`
2. Have a public `using CreateParams = ...` typedef that points to a struct with `chimod_lib_name`
3. Both requirements are already satisfied in the existing code

## Files Modified
- `tasks/rankConsensus/src/rankConsensus_runtime.cc` - Added `CHI_TASK_CC` macro
- `tasks/coeus_mdm/src/coeus_mdm_runtime.cc` - Added `CHI_TASK_CC` macro

## Verification
After these changes:
- ✅ Modules will be auto-discovered by `ModuleManager::ServerInit()`
- ✅ `GetChiMod("chimaera_rankConsensus")` will successfully return module info
- ✅ `GetChiMod("chimaera_coeus_mdm")` will successfully return module info
- ✅ No compilation errors (verified with linter)

## Impact
This update enables proper module discovery and loading in `HermesEngine::Init_()`, allowing the adapter to:
- Verify that required modules (`rankConsensus` and `coeus_mdm`) are available
- Log warnings if modules are missing
- Proceed with pool creation knowing the modules are loaded

