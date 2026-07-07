# Refactoring Plan: Replace CTETag with IHermes-based CTE Client

## Overview
Refactor `HermesEngine` to use `IHermes*` pointer instead of `std::unique_ptr<CTETag>`, where the `IHermes` implementation wraps `wrp_cte::core::Client` directly.

## Current Architecture
- `HermesEngine` uses `std::unique_ptr<coeus::CTETag> current_tag`
- `CTETag` wraps `wrp_cte::core::Tag` which internally uses `WRP_CTE_CLIENT`
- `IHermes` interface exists but is not fully utilized in `HermesEngine`

## Target Architecture
- `HermesEngine` uses `IHermes* hermes_` pointer
- New `CTEHermes` class implements `IHermes` interface
- `CTEHermes` wraps `wrp_cte::core::Client*` directly
- `CTEHermes::GetTag()` creates/manages `ITag*` instances using CTE client

## Step-by-Step Plan

### Phase 1: Create CTEHermes Implementation

#### Step 1.1: Create `include/comms/CTEHermes.h`
```cpp
#ifndef COEUS_INCLUDE_COMMS_CTEHERMES_H_
#define COEUS_INCLUDE_COMMS_CTEHERMES_H_

#include "interfaces/IHermes.h"
#include "interfaces/ITag.h"
#include <wrp_cte/core/core_client.h>
#include <wrp_cte/core/core_tasks.h>
#include <memory>
#include <string>
#include <unordered_map>

namespace coeus {

/**
 * CTEHermes: CTE-based implementation of IHermes interface
 * 
 * Wraps wrp_cte::core::Client directly and manages tags/blobs
 * using the CTE client API.
 */
class CTEHermes : public IHermes {
 public:
  /**
   * Constructor
   * @param cte_client Pointer to CTE client (uses WRP_CTE_CLIENT if nullptr)
   */
  explicit CTEHermes(wrp_cte::core::Client* cte_client = nullptr);

  /**
   * Destructor
   */
  ~CTEHermes() override;

  /**
   * Connect/Initialize CTE (if not already initialized)
   * @return true if successful, false otherwise
   */
  bool connect() override;

  /**
   * Get or create a tag by name
   * Creates an ITag wrapper that uses CTE client for operations
   * @param tag_name Name of the tag
   * @return true if successful, false otherwise
   */
  bool GetTag(const std::string &tag_name) override;

  /**
   * Demote blob (CTE handles automatically via scoring)
   * @param tag_name Name of the tag
   * @param blob_name Name of the blob
   * @return true (no-op, CTE handles automatically)
   */
  bool Demote(const std::string &tag_name, const std::string &blob_name) override;

  /**
   * Prefetch blob (CTE handles automatically via scoring)
   * @param tag_name Name of the tag
   * @param blob_name Name of the blob
   * @return true (no-op, CTE handles automatically)
   */
  bool Prefetch(const std::string &tag_name, const std::string &blob_name) override;

 private:
  wrp_cte::core::Client* cte_client_;  // CTE client (owned or borrowed)
  bool owns_client_;                    // Whether we own the client
  std::unordered_map<std::string, std::unique_ptr<ITag>> tag_cache_;  // Cache of created tags
};

} // namespace coeus

#endif // COEUS_INCLUDE_COMMS_CTEHERMES_H_
```

#### Step 1.2: Create `src/CTEHermes.cc`
- Implement constructor/destructor
- Implement `connect()` - ensure CTE is initialized
- Implement `GetTag()` - create CTETagClient wrapper that uses CTE client directly
- Implement `Demote()`/`Prefetch()` as no-ops

#### Step 1.3: Create `include/comms/CTETagClient.h` (new ITag implementation)
```cpp
/**
 * CTETagClient: ITag implementation using wrp_cte::core::Client directly
 * 
 * Unlike CTETag which wraps wrp_cte::core::Tag, this class uses
 * the CTE client API directly for tag/blob operations.
 */
class CTETagClient : public ITag {
 public:
  CTETagClient(wrp_cte::core::Client* cte_client, const std::string& tag_name);
  ~CTETagClient() override;
  
  void Put(const std::string &blob_name, size_t blob_size, const void* values) override;
  std::vector<uint8_t> Get(const std::string &blob_name) override;
  std::vector<std::string> GetContainedBlobNames() override;
  size_t GetBlobSize(const std::string &blob_name) override;

 private:
  wrp_cte::core::Client* cte_client_;
  wrp_cte::core::TagId tag_id_;
  std::string tag_name_;
};
```

### Phase 2: Update HermesEngine

#### Step 2.1: Update `include/coeus/HermesEngine.h`
- Replace: `std::unique_ptr<coeus::CTETag> current_tag;`
- With: `IHermes* hermes_;` or `std::unique_ptr<IHermes> hermes_;`
- Remove: `#include <comms/CTETag.h>`
- Add: `#include <comms/interfaces/IHermes.h>`

#### Step 2.2: Update `src/hermes_engine.cc` - Constructor
- Initialize `hermes_` pointer in constructors
- Create `CTEHermes` instance: `hermes_ = new coeus::CTEHermes(WRP_CTE_CLIENT);`
- Call `hermes_->connect()` if needed

#### Step 2.3: Update `src/hermes_engine.cc` - BeginStep()
- Replace: `current_tag = std::make_unique<coeus::CTETag>(tag_name);`
- With: `hermes_->GetTag(tag_name);`

#### Step 2.4: Update all `current_tag->Put()` calls
- Replace: `current_tag->Put(name, size, values);`
- With: `hermes_->tag->Put(name, size, values);`

#### Step 2.5: Update all `current_tag->Get()` calls
- Replace: `auto blob = current_tag->Get(name);`
- With: `auto blob = hermes_->tag->Get(name);`

#### Step 2.6: Update all `current_tag->name` references
- Replace: `current_tag->name`
- With: `hermes_->tag->name`

#### Step 2.7: Update `src/hermes_engine.cc` - EndStep()
- Replace: `current_tag.reset();`
- With: `hermes_->tag = nullptr;` (or clear tag in CTEHermes)

#### Step 2.8: Update `src/hermes_engine.cc` - Destructor
- Add cleanup: `delete hermes_;` if using raw pointer

### Phase 3: Update Hermes.h (if still used)

#### Step 3.1: Update `include/comms/Hermes.h`
- Modify `GetTag()` to work with new architecture
- Ensure compatibility with existing code

### Phase 4: Testing & Validation

#### Step 4.1: Compile and fix errors
- Ensure all includes are correct
- Fix any API mismatches

#### Step 4.2: Verify functionality
- Test Put operations
- Test Get operations
- Test tag creation/management
- Verify metadata operations still work

## Implementation Details

### CTETagClient Implementation Notes
- Uses `cte_client->AsyncGetOrCreateTag()` to get/create tag
- Uses `cte_client->AsyncPutBlob()` for Put operations
- Uses `cte_client->AsyncGetBlob()` for Get operations
- Manages shared memory allocation/deallocation
- Handles async operations with Wait()

### CTEHermes Implementation Notes
- Can use global `WRP_CTE_CLIENT` or accept client pointer
- Caches created tags in `tag_cache_` map
- `GetTag()` returns cached tag or creates new one
- Manages tag lifecycle

### Memory Management
- Option A: Use raw pointer `IHermes* hermes_` - manage in destructor
- Option B: Use `std::unique_ptr<IHermes> hermes_` - automatic cleanup
- Recommendation: Use `std::unique_ptr<IHermes>` for safety

## Files to Modify

### New Files
1. `include/comms/CTEHermes.h` - New IHermes implementation
2. `src/CTEHermes.cc` - Implementation
3. `include/comms/CTETagClient.h` - New ITag implementation using CTE client
4. `src/CTETagClient.cc` - Implementation

### Modified Files
1. `include/coeus/HermesEngine.h` - Replace CTETag with IHermes
2. `src/hermes_engine.cc` - Update all CTETag usage to IHermes
3. `include/comms/Hermes.h` - Update if needed
4. `src/CMakeLists.txt` - Add new source files

### Files to Consider Removing
1. `include/comms/CTETag.h` - May become obsolete if CTETagClient replaces it
2. `src/CTETag.cc` - May become obsolete

## Migration Strategy

### Option 1: Big Bang (Recommended)
- Implement all phases at once
- Test thoroughly
- Clean up old code

### Option 2: Incremental
- Phase 1: Create CTEHermes, keep CTETag working
- Phase 2: Update HermesEngine to use IHermes
- Phase 3: Remove CTETag if no longer needed

## Benefits
1. **Abstraction**: Better separation of concerns
2. **Flexibility**: Can swap IHermes implementations
3. **Direct CTE Access**: Uses CTE client directly without Tag wrapper
4. **Consistency**: Uses IHermes interface throughout
5. **Testability**: Easier to mock IHermes for testing

## Risks & Considerations
1. **Breaking Changes**: May affect existing code
2. **Performance**: Need to verify no performance regression
3. **Memory Management**: Ensure proper cleanup
4. **Tag Caching**: Consider if tag caching is needed
5. **Error Handling**: Ensure proper error propagation

## Success Criteria
- [ ] All compilation errors resolved
- [ ] All Put/Get operations work correctly
- [ ] Tag creation/management works
- [ ] No memory leaks
- [ ] Performance is acceptable
- [ ] Code is cleaner and more maintainable

