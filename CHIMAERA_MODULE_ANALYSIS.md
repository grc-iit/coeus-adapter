# Chimaera Module Usage Analysis

## Summary
Analysis of `hermes_engine.cc` and module implementations (`rankConsensus`, `coeus_mdm`) against the Chimaera Module Development Guide.

## Critical Issues Found

### 1. ❌ **CRITICAL: Missing `pool_id_` Update After Create**

**Location:** `tasks/rankConsensus/include/chimaera/rankConsensus/rankConsensus_client.h:58-67`  
**Location:** `tasks/coeus_mdm/include/chimaera/coeus_mdm/coeus_mdm_client.h:62-72`

**Problem:** The synchronous `Create()` wrappers do NOT update `pool_id_` with the returned `new_pool_id_` from the completed task.

**Guide Requirement:**
> **CRITICAL**: All ChiMod clients implementing Create functions MUST update their `pool_id_` field with the actual pool ID returned from completed CreateTask operations.

**Current Code:**
```cpp
void Create(const chi::PoolQuery& pool_query,
            const std::string& pool_name,
            const chi::PoolId& custom_pool_id) {
  auto future = AsyncCreate(pool_query, pool_name, custom_pool_id);
  future.Wait();
  if (future->GetReturnCode() != 0) {
    HLOG(kError, "rankConsensus::Create failed with return code: {}",
          future->GetReturnCode());
  }
  // ❌ MISSING: pool_id_ = future->new_pool_id_;
}
```

**Impact:** Subsequent operations (like `GetRank()`, `Mdm_insert()`) may use the wrong pool ID, causing:
- Tasks sent to incorrect/non-existent pools
- Deadlocks when multiple clients try to use the same pool
- Scale-dependent failures (explains why 128 ranks fails but 32/64 work)

**Fix Required:**
```cpp
void Create(const chi::PoolQuery& pool_query,
            const std::string& pool_name,
            const chi::PoolId& custom_pool_id) {
  auto future = AsyncCreate(pool_query, pool_name, custom_pool_id);
  future.Wait();
  if (future->GetReturnCode() != 0) {
    HLOG(kError, "rankConsensus::Create failed with return code: {}",
          future->GetReturnCode());
    return; // Early return on error
  }
  // ✅ CRITICAL: Update pool_id_ with actual pool ID from task
  pool_id_ = future->new_pool_id_;
}
```

---

### 2. ⚠️ **Suboptimal: Using `PoolQuery::Local()` Instead of `Dynamic()`**

**Location:** `src/hermes_engine.cc:201, 213, 277`

**Problem:** All Create operations use `chi::PoolQuery::Local()` instead of the recommended `chi::PoolQuery::Dynamic()`.

**Guide Recommendation:**
> **Default to Dynamic for Create**: Use `PoolQuery::Dynamic()` for container creation to enable automatic caching optimization.

**Current Code:**
```cpp
// Line 201
auto admin_create_task = admin_client.AsyncCreate(chi::PoolQuery::Local(), "admin", chi::kAdminPoolId);

// Line 213
rank_consensus.Create(chi::PoolQuery::Local(), "rankConsensus", rankConsensus_pool_id_);

// Line 277
client.Create(chi::PoolQuery::Local(), "db_operation", coeus_mdm_pool_id_, db_file);
```

**Impact:**
- No automatic caching optimization
- Potentially slower pool lookups at scale
- May contribute to performance degradation at 128 ranks

**Fix Recommended:**
```cpp
// Use Dynamic() for Create operations
auto admin_create_task = admin_client.AsyncCreate(chi::PoolQuery::Dynamic(), "admin", chi::kAdminPoolId);
rank_consensus.Create(chi::PoolQuery::Dynamic(), "rankConsensus", rankConsensus_pool_id_);
client.Create(chi::PoolQuery::Dynamic(), "db_operation", coeus_mdm_pool_id_, db_file);
```

---

### 3. ⚠️ **Potential Issue: Admin Client Create Pattern**

**Location:** `src/hermes_engine.cc:200-207`

**Problem:** Admin client is created with `chi::kAdminPoolId`, then Create is called, then `Init()` is called separately.

**Current Code:**
```cpp
chimaera::admin::Client admin_client(chi::kAdminPoolId);
auto admin_create_task = admin_client.AsyncCreate(chi::PoolQuery::Local(), "admin", chi::kAdminPoolId);
admin_create_task.Wait();
if (admin_create_task->GetReturnCode() != 0) {
  engine_logger->error("Failed to create admin container");
  throw coeus::common::ErrorException(HERMES_CONNECT_FAILED);
}
admin_client.Init(admin_create_task->new_pool_id_);
```

**Analysis:** 
- Admin pool should already exist (it's a system pool)
- Creating it again may be redundant
- The `Init()` call updates `pool_id_`, which is correct

**Recommendation:** Verify if admin pool needs explicit creation or if it's auto-initialized. If admin pool is guaranteed to exist, this Create may be unnecessary.

---

### 4. ✅ **Correct: Using `chi::kAdminPoolId` in AsyncCreate**

**Location:** `tasks/rankConsensus/include/chimaera/rankConsensus/rankConsensus_client.h:41`  
**Location:** `tasks/coeus_mdm/include/chimaera/coeus_mdm/coeus_mdm_client.h:43`

**Status:** ✅ **CORRECT**

Both clients correctly use `chi::kAdminPoolId` when constructing CreateTask operations, as required by the guide:

```cpp
auto task = ipc_manager->NewTask<CreateTask>(
    chi::CreateTaskId(),
    chi::kAdminPoolId,  // ✅ CORRECT: Always use admin pool for CreateTask
    pool_query,
    CreateParams::chimod_lib_name,
    pool_name,
    custom_pool_id,
    this
);
```

---

### 5. ✅ **Correct: Module Structure and Runtime Implementation**

**Status:** ✅ **CORRECT**

Both modules follow the correct structure:
- ✅ Proper `chimaera_mod.yaml` configuration
- ✅ Runtime inherits from `chi::Container`
- ✅ Client inherits from `chi::ContainerClient`
- ✅ `CHI_TASK_CC` macro used correctly
- ✅ Task definitions follow guide patterns
- ✅ Runtime methods properly implemented

---

## L-Dependent Hang (L=64 OK, L=128/256 Hang)

**Observed:** Hang occurs when Gray-Scott domain size **L > 64** (e.g. L=128, 256), not just when rank count is high. L=64 runs fine; L=128 or 256 causes hang with Hermes enabled.

**Why L matters:**

- Local grid per rank scales with L: `size_x ≈ L/npx`, `size_y ≈ L/npy`, `size_z ≈ L/npz`.
- **Per-rank data size** (U, V) ≈ `size_x * size_y * size_z` doubles → scales as **L³ / procs**.
- **Halo message size** (e.g. xy face) ≈ `(size_y+2)*size_x` doubles → scales as **L² / procs**.

Approximate per-rank sizes (128 ranks, 4×4×8 or 5×5×5):

| L   | Local per dim (approx) | U/V per rank   | Blob Put size (U+V) |
|-----|------------------------|----------------|---------------------|
| 64  | ~16                   | ~4K doubles    | ~64 KB              |
| 128 | ~32                   | ~32K doubles   | ~512 KB             |
| 256 | ~64                   | ~262K doubles  | ~4 MB               |

**Likely causes when L > 64:**

1. **Large-blob CTE/Hermes path (most likely)**  
   `CTETagClient::Put()` does `AllocateBuffer(blob_size)` then `AsyncPutBlob(..., blob_size, ...)`. For L=128/256, blob_size is hundreds of KB to several MB per rank. Possible issues:
   - **SHM exhaustion**: Many ranks allocating large buffers at once → `AllocateBuffer` blocks or fails.
   - **CTE PutBlob**: Different code path or timeout for large blobs → task never completes → `task.Wait()` hangs.
   - **Memory pressure**: Large allocs change timing and expose races (e.g. pool_id_ or concurrent Create).

2. **MPI / halo exchange**  
   Larger L → larger derived datatypes and message sizes. Possible but less likely as sole cause (MPI_Sendrecv is standard); more likely as a secondary effect if Hermes holds resources and delays progress.

3. **SQLite / coeus_mdm**  
   Larger L → more or larger metadata. Lock contention or serialization could slow or block many ranks.

**Diagnostics to run:**

- **Isolate I/O vs exchange:** Run with **`COEUS_DISABLE_CTE_IO=1`** at L=128 (and 128 ranks).  
  - If **hang goes away** → hang is in Hermes/CTE (large blob Put or related SHM/CTE path).  
  - If **still hangs** → problem is elsewhere (e.g. MPI, rankConsensus, or init).
- **Isolate exchange:** Run with **L=128** but **skip writer** (e.g. set `plotgap` larger than steps so no write).  
  - If **no hang** → hang is in write path (Hermes/CTE) when L is large.
- **Log blob sizes:** In HermesEngine `DoPutSync_`/`DoPutDeferred_`, log `variable.SelectionSize() * sizeof(T)` (and L/rank) to confirm threshold (e.g. hang above ~100KB or ~1MB per rank).

**Mitigations to consider:**

- **pool_id_ fix** (already done): Ensures correct pool usage at any scale.
- **Limit or batch large Puts**: If SHM or CTE has limits, cap blob size per Put or split into chunks (would require API support).
- **Increase SHM pool / reduce concurrency**: If the issue is SHM exhaustion, increase shared memory or stagger Put calls (e.g. barrier after exchange, then write).

---

## Root Cause Hypothesis for 128-Rank Hang

The **missing `pool_id_` update** (Issue #1) is the most likely root cause:

1. **At small scales (32/64 ranks):**
   - Fewer concurrent Create operations
   - Race conditions less likely
   - May accidentally work if pool IDs happen to match

2. **At 128 ranks:**
   - Many concurrent Create operations
   - Multiple clients trying to use pools with wrong IDs
   - Tasks sent to wrong pools → deadlock in MPI communication
   - Explains deterministic hang during halo exchange (MPI operations blocked waiting for Chimaera tasks that never complete)

3. **Why it hangs in `exchange()`:**
   - If rankConsensus or coeus_mdm operations are still pending (due to wrong pool IDs)
   - And those operations involve MPI communication (via Chimaera runtime)
   - Then MPI_Sendrecv can deadlock waiting for Chimaera tasks that are stuck

**Combined with L:** For **L > 64**, per-rank blob size and SHM usage grow (L³ effect). That can make wrong-pool or resource exhaustion show up earlier (e.g. exactly at L=128). So both the **pool_id_** fix and **L-dependent (large-blob) behavior** should be addressed.

---

## Recommended Fix Priority

1. **🔴 CRITICAL (Fix Immediately):**
   - Update `pool_id_` in both `rankConsensus::Client::Create()` and `coeus_mdm::Client::Create()`

2. **🟡 HIGH (Fix Soon):**
   - Change `PoolQuery::Local()` to `PoolQuery::Dynamic()` for all Create operations

3. **🟢 LOW (Investigate):**
   - Review admin client Create pattern (may be redundant)

---

## Files to Fix

1. `tasks/rankConsensus/include/chimaera/rankConsensus/rankConsensus_client.h` (line 58-67)
2. `tasks/coeus_mdm/include/chimaera/coeus_mdm/coeus_mdm_client.h` (line 62-72)
3. `src/hermes_engine.cc` (lines 201, 213, 277)

---

## Testing After Fix

1. Run Gray-Scott at 128 ranks with Hermes enabled
2. Verify all ranks complete rankConsensus successfully
3. Check that `pool_id_` values are correct after Create
4. Monitor for any remaining deadlocks
