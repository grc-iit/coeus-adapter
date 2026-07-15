[← COEUS-Adapter README](../README.md)

# Derived Variables and Hash

COEUS computes **ADIOS2 derived variables in-situ**, once per step, as data
flows through the engine. The producer declares them with the standard ADIOS2
API — COEUS reads the source fields from CTE, applies the expression, and stores
the result alongside the raw data:

```cpp
// variance / add (used by the trigger pipeline)
io.DefineDerivedVariable("derive/VarV", "x = V \n variance(x)",
                         adios2::DerivedVarType::StoreData);
// content hash of a field
io.DefineDerivedVariable("derive/hashV", "x = V \n hash(x)",
                         adios2::DerivedVarType::StoreData);
```

Supported operations include `curl`, `magnitude`, `variance`, `add`, `mean`
(custom), and **`hash`**.

The derived `variance` / `mean` variables above are exactly what the
[Trigger-Render-Reason pipeline](TRIGGER_RENDER_REASON_PIPELINE.md) monitors.

## `hash()`

`hash()` is a derived-variable operation for content hashing / deduplication of
floating-point snapshots. Importantly, **it is implemented inside ADIOS2**
(the state-diff-enabled ADIOS2 fork, using Kokkos + DataStates state-diff) — so
COEUS reaches it through the same derived-variable path as everything else and
links **no** extra libraries. To enable it you only define the derived variable
above and build against the state-diff ADIOS2 fork.

➡️  **See [addons/HASH.md](../addons/HASH.md)** for the full explanation, the
in-tree `adios_hashing` example, and the RECUP background.
