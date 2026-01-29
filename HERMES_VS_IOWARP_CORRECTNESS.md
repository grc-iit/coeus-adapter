# Hermes Engine vs IowarpEngine Correctness Check

Reference: IowarpEngine (user-provided). Checked: `src/hermes_engine.cc`, `include/comms/CTEHermes.cc`, `include/comms/CTETagClient.cc`.

---

## Summary

- **CTEHermes.cc**: Correct. CTE connect flow (WRP_CTE_CLIENT_INIT, pre-deployed vs create, pool_id_, Init, RegisterTarget) and GetTag/Demote/Prefetch match expected behavior.
- **hermes_engine.cc**: One fix applied (DoClose should clear tag). One semantic note: DoPutDeferred_ is synchronous (no deferred tasks / EndStep wait like IowarpEngine).
- **CTETagClient.cc**: Correct. Put/Get use tag_id_, SHM, AsyncPutBlob/GetBlob, Wait; no compression context (minor vs reference).

---

## 1. CTE Initialization and Pool Attachment

| Aspect | IowarpEngine (reference) | HermesEngine / CTEHermes |
|--------|---------------------------|---------------------------|
| Where CTE init runs | Constructor: `WRP_CTE_CLIENT_INIT("", chi::PoolQuery::Local())` only | Constructor: `hermes_->connect()` which does `WRP_CTE_CLIENT_INIT` + pool attach |
| Pool / Init(pool_id) | Not done in engine; assumes pre-deployed or default | **CTEHermes::connect()**: `CTE_PRE_DEPLOYED=1` → set `pool_id_ = kCtePoolId`, `Init(kCtePoolId)`; else AsyncCreate, set `pool_id_` from task, `Init(new_pool_id_)`, RegisterTarget |

**Verdict:** CTEHermes is correct and more complete: it explicitly attaches the client to the correct pool (pre-deployed or created), avoiding garbage pool_id and "Container not found" / segfaults. IowarpEngine relies on something else (e.g. pre-deployed + global client) or default pool.

---

## 2. Tag Creation and Naming

| Aspect | IowarpEngine | HermesEngine |
|--------|--------------|--------------|
| When tag is created | `Init_()`: `current_tag_ = std::make_unique<wrp_cte::core::Tag>(m_Name)` (engine name) | `BeginStep()`: `GetTag(tag_name)` with `tag_name = "step_" + currentStep + "_rank" + rank` |
| Tag type | `wrp_cte::core::Tag` (CTE Tag object) | `CTETagClient(cte_client_, tag_name)` (ITag wrapper using Client API) |
| Blob naming | Single tag; blob = `variable.m_Name + "_step_" + current_step_ + "_rank_" + rank_` | Per-step-per-rank tag; blob = variable name only (e.g. "U", "V") |

**Verdict:** Both designs are consistent. HermesEngine: tag = step+rank, blob = variable name. IowarpEngine: tag = engine name, blob = var_step_rank.

---

## 3. Put / Get Semantics

| Aspect | IowarpEngine | HermesEngine / CTETagClient |
|--------|--------------|-----------------------------|
| Sync Put | `current_tag_->PutBlob(blob_name, data, data_size, 0, 1.0f, context)` (Tag API, raw ptr + context) | `hermes_->tag->Put(name, blob_size, values)` → CTETagClient::Put: allocate SHM, copy, AsyncPutBlob, Wait, FreeBuffer |
| Deferred Put | Allocates SHM, copies, `AsyncPutBlob`, stores `DeferredTask{task, buffer}`; **EndStep** sets TASK_DATA_OWNER and **Wait()** on all deferred tasks | **DoPutDeferred_** calls same `hermes_->tag->Put(...)` (sync). No deferred list, no EndStep wait |
| Get | `current_tag_->GetBlob(blob_name, buffer, expected_size, 0)` or deferred → sync Get | `hermes_->tag->Get(blob_name)` → CTETagClient::Get (GetBlobSize, AllocateBuffer, AsyncGetBlob, Wait, copy, FreeBuffer); DoGetDeferred_ calls DoGetSync_ |

**Verdict:** Put/Get behavior is correct. HermesEngine "deferred" put is effectively synchronous (no overlap like IowarpEngine’s deferred tasks + EndStep wait). This is a semantic/performance difference, not a correctness bug. Optional improvement: implement true deferred put (store task+buffer, wait in EndStep) to match reference.

---

## 4. Step and Close Lifecycle

| Aspect | IowarpEngine | HermesEngine |
|--------|--------------|--------------|
| BeginStep | Increment step; lazy `Init_()` if !open_ | Increment step; `GetTag("step_" + currentStep + "_rank" + rank)` (new tag per step) |
| EndStep | Process deferred tasks (set TASK_DATA_OWNER, Wait), clear deferred_tasks_ | ComputeDerivedVariables; delete hermes_->tag, set nullptr |
| DoClose | `current_tag_.reset(); open_ = false` | **Before fix:** only `open = false` (tag left set). **After fix:** clear hermes_->tag then set open = false |

**Verdict:** DoClose should clear the tag to match reference and avoid any use of tag after close. Fix applied below.

---

## 5. CTETagClient vs wrp_cte::core::Tag

- **CTETagClient**: Uses `cte_client_->AsyncGetOrCreateTag`, then `AsyncPutBlob(tag_id_, blob_name, 0, blob_size, shm_ptr, GetDefaultBlobScore(), 0)`, AsyncGetBlobSize + AsyncGetBlob. Correct use of Client API and pool_id_ set in connect().
- **IowarpEngine** uses `wrp_cte::core::Tag(m_Name)` which internally uses WRP_CTE_CLIENT and GetOrCreateTag; Tag::PutBlob allocates SHM, copies, AsyncPutBlob, Wait, FreeBuffer — same pattern as CTETagClient::Put.
- **Compression:** IowarpEngine passes a Context from env (CreateCompressionContext). CTETagClient passes flags=0. Optional enhancement: add env-driven compression context to CTETagClient if needed.

---

## 6. Chimaera Init Order (HermesEngine)

- **Constructor:** `hermes_->connect()` → WRP_CTE_CLIENT_INIT + pool Init.
- **Init_():** CHIMAERA_INIT, ModuleManager, admin Create, rankConsensus Create, etc.

If WRP_CTE_CLIENT_INIT already initializes Chimaera, CHIMAERA_INIT in Init_() may be redundant for CTE; it is still required for admin and rankConsensus pools. No change recommended; ordering is correct.

---

## 7. Fix Applied

**HermesEngine::DoClose** — clear tag on close (match IowarpEngine):

- Before: Only set `open = false`; hermes_->tag was left set.
- After: If hermes_ and hermes_->tag are set, delete hermes_->tag and set hermes_->tag = nullptr; then set open = false.

This avoids any use of tag after close and aligns with the reference engine.
