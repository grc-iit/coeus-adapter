# I/O Engine Migration Plan: Hermes to Context-Transfer-Engine

## Overview

This document outlines the migration plan for replacing Hermes blob storage I/O operations with Context-Transfer-Engine (CTE) for intelligent data placement across storage tiers.

## Current Architecture

### Hermes I/O System
- **Storage Backend**: Hermes bucket-based blob storage
- **API**: `Hermes->bkt->Put(blob_name, size, data)` and `Hermes->bkt->Get(blob_name)`
- **Organization**: Buckets (namespaces) containing blobs
- **Interface**: `IBucket` interface with `Bucket` implementation
- **Usage**: Direct blob operations in `HermesEngine::DoPutSync_`, `DoPutDeferred_`, `DoGetSync_`, `DoGetDeferred_`

### Key Files
- `include/comms/Bucket.h` - Bucket wrapper around Hermes API
- `include/comms/interfaces/IBucket.h` - Bucket interface
- `include/comms/Hermes.h` - Hermes connection management
- `src/hermes_engine.cc` - ADIOS2 engine using Hermes I/O

## Target Architecture: Context-Transfer-Engine

### CTE I/O System
- **Storage Backend**: CTE Core with multi-tier storage support
- **API**: `Tag::PutBlob(blob_name, data, size)` and `Tag::GetBlob(blob_name, data, size)`
- **Organization**: Tags (like buckets) containing blobs with intelligent placement
- **Features**:
  - Multi-tier storage (RAM, NVMe, SSD, HDD)
  - Blob scoring for data temperature
  - Automatic data placement optimization
  - Shared memory for zero-copy transfers
- **Initialization**: `WRP_CTE_CLIENT_INIT(config_path)`

### Key CTE Components
- `wrp_cte::core::Client` - Main CTE client
- `wrp_cte::core::Tag` - Tag wrapper for convenient blob operations
- `WRP_CTE_CLIENT` - Global CTE client singleton
- `WRP_CTE_CLIENT_INIT()` - Initialization function

## Migration Strategy

### Phase 1: Create CTE Bucket Implementation

**Goal**: Create a new `CTEBucket` class that implements `IBucket` interface using CTE backend.

**Files to Create/Modify**:
1. `include/comms/CTEBucket.h` - New CTE-based bucket implementation
2. `src/CTEBucket.cc` - Implementation file

**Design**:
```cpp
namespace coeus {
class CTEBucket : public IBucket {
private:
  wrp_cte::core::Tag tag_;  // CTE tag (replaces Hermes bucket)
  std::string bucket_name_; // Original bucket name for compatibility
  
public:
  CTEBucket(const std::string &bucket_name);
  
  void Put(const std::string &blob_name, size_t blob_size, const void* values) override;
  hermes::Blob Get(const std::string &blob_name) override;
  // ... other interface methods
};
}
```

**Key Considerations**:
- Map bucket names to CTE tag names (1:1 mapping)
- Convert `hermes::Blob` return type to maintain interface compatibility
- Handle shared memory allocation/deallocation for CTE operations
- Support blob scoring (can use default or configurable)

### Phase 2: Update Hermes Class

**Goal**: Modify `Hermes` class to support CTE initialization and bucket creation.

**Files to Modify**:
1. `include/comms/Hermes.h`
2. `src/Hermes.cc` (if exists, or add to header)

**Changes**:
- Add CTE initialization in `connect()` method
- Add flag to choose between Hermes and CTE backends (for transition period)
- Modify `GetBucket()` to return `CTEBucket` instead of `Bucket` when CTE is enabled
- Keep Hermes initialization for backward compatibility (optional)

### Phase 3: Update HermesEngine

**Goal**: Integrate CTE initialization and ensure proper lifecycle management.

**Files to Modify**:
1. `include/coeus/HermesEngine.h`
2. `src/hermes_engine.cc`

**Changes**:
- Initialize CTE in `HermesEngine::Init_()` using `WRP_CTE_CLIENT_INIT()`
- Register storage targets if needed (or use config file)
- Ensure CTE is initialized before bucket operations
- Add CTE configuration support (YAML config file path)

### Phase 4: Update CMake Configuration

**Goal**: Add CTE dependencies to build system.

**Files to Modify**:
1. `CMakeLists.txt` (root)
2. `src/CMakeLists.txt`

**Changes**:
- Add `find_package(wrp_cte_core REQUIRED)`
- Link against `wrp_cte::core_client` target
- Ensure CTE headers are available

### Phase 5: Configuration and Testing

**Goal**: Create CTE configuration and validate migration.

**Files to Create**:
1. `config/cte_config.yaml` - CTE storage configuration

**Configuration Example**:
```yaml
worker_count: 4

storage:
  # Primary high-performance storage
  - path: "/mnt/nvme/cte_primary"
    bdev_type: "file"
    capacity_limit: "1TB"
    score: 0.9
  
  # RAM-based cache
  - path: "/tmp/cte_cache"
    bdev_type: "ram"
    capacity_limit: "8GB"
    score: 1.0

dpe:
  dpe_type: "max_bw"  # max_bw, round_robin, or random
```

## Implementation Details

### 1. CTEBucket Implementation

**Put Operation**:
```cpp
void CTEBucket::Put(const std::string &blob_name, size_t blob_size, const void* values) {
  try {
    // CTE Tag::PutBlob handles shared memory automatically for sync operations
    tag_.PutBlob(blob_name, static_cast<const char*>(values), blob_size);
  } catch (const std::exception& e) {
    // Handle error - convert to appropriate error code or throw
    throw std::runtime_error("CTE PutBlob failed: " + std::string(e.what()));
  }
}
```

**Get Operation**:
```cpp
hermes::Blob CTEBucket::Get(const std::string &blob_name) {
  try {
    // Get blob size first
    chi::u64 blob_size = tag_.GetBlobSize(blob_name);
    if (blob_size == 0) {
      return hermes::Blob(); // Return empty blob
    }
    
    // Allocate buffer for data
    std::vector<char> buffer(blob_size);
    
    // Retrieve blob data
    tag_.GetBlob(blob_name, buffer.data(), blob_size);
    
    // Convert to hermes::Blob for interface compatibility
    hermes::Blob blob(blob_size);
    memcpy(blob.data(), buffer.data(), blob_size);
    return blob;
  } catch (const std::exception& e) {
    // Handle error
    return hermes::Blob(); // Return empty blob on error
  }
}
```

### 2. Hermes Class Updates

**Connect Method**:
```cpp
bool Hermes::connect() override {
  // Initialize CTE (replaces or supplements Hermes)
  std::string cte_config = getenv("CTE_CONFIG") ? getenv("CTE_CONFIG") : "";
  if (cte_config.empty()) {
    cte_config = "config/cte_config.yaml"; // Default config path
  }
  
  // WRP_CTE_CLIENT_INIT automatically calls chi::CHIMAERA_INIT internally
  bool cte_init = wrp_cte::core::WRP_CTE_CLIENT_INIT(cte_config);
  if (!cte_init) {
    std::cerr << "Failed to initialize CTE" << std::endl;
    return false;
  }
  
  // Optional: Keep Hermes for backward compatibility if needed
  // TRANSPARENT_HERMES();
  // hermes = HERMES;
  
  return true;
}
```

**GetBucket Method**:
```cpp
bool Hermes::GetBucket(const std::string &bucket_name) override {
  // Create CTE bucket instead of Hermes bucket
  bkt = (IBucket*) new coeus::CTEBucket(bucket_name);
  return true;
}
```

### 3. HermesEngine Initialization

**Init_ Method Updates**:
```cpp
void HermesEngine::Init_() {
  // ... existing initialization code ...
  
  // Initialize CTE for I/O operations
  // Note: This may already be done in Hermes::connect(), but ensure it's done
  std::string cte_config = params.get("cte_config", std::string(""));
  if (cte_config.empty()) {
    cte_config = getenv("CTE_CONFIG") ? getenv("CTE_CONFIG") : "config/cte_config.yaml";
  }
  
  // CTE initialization (if not already done)
  if (!WRP_CTE_CLIENT) {
    wrp_cte::core::WRP_CTE_CLIENT_INIT(cte_config);
  }
  
  // ... rest of initialization ...
}
```

### 4. Blob Scoring Strategy

**Considerations**:
- Use default score (1.0) for all blobs initially
- Can add scoring based on:
  - Variable access frequency
  - Variable size
  - Step number (recent data = higher score)
  - User-provided hints

**Example Scoring**:
```cpp
float CalculateBlobScore(const std::string &var_name, size_t step) {
  // Simple scoring: recent steps get higher scores
  float base_score = 0.5f;
  float recency_bonus = std::min(0.5f, (currentStep - step) * 0.1f);
  return std::min(1.0f, base_score + recency_bonus);
}
```

## Migration Steps

### Step 1: Create CTEBucket Class
1. Create `include/comms/CTEBucket.h`
2. Create `src/CTEBucket.cc`
3. Implement `IBucket` interface using CTE Tag API
4. Handle memory management and error cases

### Step 2: Update Build System
1. Add CTE dependencies to `CMakeLists.txt`
2. Link against `wrp_cte::core_client`
3. Ensure CTE headers are accessible

### Step 3: Modify Hermes Class
1. Add CTE initialization to `connect()`
2. Update `GetBucket()` to return `CTEBucket`
3. Add configuration support

### Step 4: Update HermesEngine
1. Ensure CTE initialization in `Init_()`
2. Add CTE configuration parameter support
3. Test I/O operations

### Step 5: Create Configuration
1. Create `config/cte_config.yaml`
2. Configure storage targets
3. Set data placement engine type

### Step 6: Testing and Validation
1. Unit tests for CTEBucket
2. Integration tests with ADIOS2
3. Performance comparison with Hermes
4. Validate data correctness

## Backward Compatibility

### Option 1: Dual Backend Support (Recommended for Transition)
- Add flag to choose between Hermes and CTE
- Allow gradual migration
- Keep Hermes code for fallback

### Option 2: Complete Replacement
- Remove Hermes I/O code entirely
- Use CTE exclusively
- Simpler but requires full validation

## Benefits of Migration

1. **Intelligent Data Placement**: Automatic tier selection based on access patterns
2. **Multi-Tier Storage**: Support for RAM, NVMe, SSD, HDD in single system
3. **Performance Optimization**: Better I/O performance through intelligent caching
4. **Scalability**: Better support for distributed storage
5. **Modern Architecture**: Aligned with Chimaera framework

## Risks and Mitigation

1. **Risk**: CTE API changes
   - **Mitigation**: Pin CTE version, test thoroughly

2. **Risk**: Performance regression
   - **Mitigation**: Benchmark before/after, optimize configuration

3. **Risk**: Data compatibility
   - **Mitigation**: Validate data correctness, maintain backup

4. **Risk**: Configuration complexity
   - **Mitigation**: Provide default configuration, document options

## Timeline Estimate

- **Phase 1** (CTEBucket): 2-3 days
- **Phase 2** (Hermes updates): 1-2 days
- **Phase 3** (HermesEngine): 1 day
- **Phase 4** (CMake): 0.5 days
- **Phase 5** (Config & Testing): 2-3 days

**Total**: ~7-10 days

## Next Steps

1. Review and approve this migration plan
2. Set up CTE development environment
3. Begin Phase 1 implementation
4. Create test cases for validation
5. Document configuration options

## References

- CTE Core Documentation: `context-transfer-engine/docs/cte.md`
- CTE API Reference: `context-transfer-engine/core/include/wrp_cte/core/core_client.h`
- Tag Wrapper: `context-transfer-engine/core/src/tag.cc`
- Current Hermes I/O: `include/comms/Bucket.h`, `src/hermes_engine.cc`

