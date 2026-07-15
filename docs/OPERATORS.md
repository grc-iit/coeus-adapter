[← COEUS-Adapter README](../README.md)

# Operators (Add-ons)

Optional, **opt-in** tools that add computation *without touching the engine* —
`src/hermes_engine.cc` is unchanged whether they are built or not. They run as
standalone **CTE consumers**: they attach to the same CTE pool and read the
per-step blobs the engine already wrote (`step_<N>_rank<r>` tags), out-of-band.

- **`coeus_tderiv`** — 4th-order centered **time derivatives** (`dp/dt`,
  `d²p/dt²`) over a 5-step sliding window. (Time derivatives need cross-step
  history, which an ADIOS2 derived variable cannot express — hence a consumer
  rather than a derived variable.)

Build them with:

```bash
cmake .. -DCOEUS_ENABLE_OPERATORS=ON
make coeus_tderiv
```

➡️  **See [addons/README.md](../addons/README.md)** for usage, offline/online
modes, and the design rationale. (`hash()` is *not* here — it is a
[derived variable](DERIVED_VARIABLES_AND_HASH.md); see
[addons/HASH.md](../addons/HASH.md).)
