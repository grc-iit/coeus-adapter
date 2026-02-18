# Catalyst Integration — Gray-Scott Example

## Overview

This example demonstrates **in-situ visualization** of the Gray-Scott reaction-diffusion simulation using ParaView Catalyst, integrated through the Coeus ADIOS2 plugin engine (`HermesEngine`). Data flows from the simulation to ParaView without touching disk, using ADIOS2's inline engine as a zero-copy bridge.

## Architecture Diagram

```
┌─────────────────────────────────────────────────────────────────────┐
│                     Gray-Scott Simulation                           │
│                  (adios2-gray-scott, MPI)                           │
│                                                                     │
│   Produces variables: U, V (3D scalar fields on Cartesian grid)     │
│   Config: setting.json  (L=64, steps=1000, plotgap=10)              │
└──────────────────────────┬──────────────────────────────────────────┘
                           │  adios2.Put(U, ...) / adios2.Put(V, ...)
                           ▼
┌─────────────────────────────────────────────────────────────────────┐
│                   ADIOS2 Plugin Engine                               │
│                    (HermesEngine)                                    │
│                                                                     │
│  adios2.xml selects engine type="Plugin" with parameters:           │
│    PluginLibrary = hermes_engine                                    │
│    DataModel     = gs-fides.json                                    │
│    Script        = catalyst.py                                      │
│    db_file       = benchmark_metadata.db                            │
│                                                                     │
│  On Init_():                                                        │
│    ┌──────────────────────────────────────────────────────────────┐  │
│    │ if "Script" AND "DataModel" params present:                 │  │
│    │   1. Create CatalystImpl state                              │  │
│    │   2. Create InlineIO (ADIOS2 inline engine)                 │  │
│    │   3. Mirror all variables (U, V) onto InlineIO              │  │
│    │   4. Open InlineWriter in Write mode                        │  │
│    │   5. Call CatalystInit() → catalyst_initialize()            │  │
│    └──────────────────────────────────────────────────────────────┘  │
└──────────────────────────┬──────────────────────────────────────────┘
                           │
            ┌──────────────┴──────────────┐
            ▼                             ▼
┌──────────────────────┐     ┌────────────────────────────────────────┐
│   CTE / Hermes       │     │         ADIOS2 Inline Engine           │
│   (Blob Store)       │     │         (InlineWriter)                 │
│                      │     │                                        │
│  hermes_->Put(name,  │     │  InlineWriter->Put(*inlineVar, values) │
│    blob_size, values) │     │  (stores raw pointer, zero-copy)      │
│                      │     │                                        │
│  Persistent data     │     │  In-memory only — no disk I/O          │
│  staging / tiering   │     │                                        │
└──────────────────────┘     └──────────────────┬─────────────────────┘
                                                │
                                                │  EndStep() triggers:
                                                │  1. InlineWriter->EndStep()
                                                │  2. CatalystExecute()
                                                ▼
┌─────────────────────────────────────────────────────────────────────┐
│                      CatalystExecute()                              │
│                                                                     │
│  Builds a Conduit node with:                                        │
│    - catalyst/state/timestep = currentStep                          │
│    - catalyst/channels/fides/type = "fides"                         │
│    - catalyst/fides/json_file = gs-fides.json                       │
│    - catalyst/fides/data_source_io/address = &InlineIO (as string)  │
│                                                                     │
│  Calls: catalyst_execute(node)                                      │
└──────────────────────────┬──────────────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────────────────┐
│                    Fides Reader (inside ParaView)                    │
│                                                                     │
│  gs-fides.json defines:                                             │
│    - VTK-Cartesian-grid with uniform point coordinates              │
│    - Dimensions derived from variable U                             │
│    - Origin: [0, 0, 0], Spacing: [0.1, 0.1, 0.1]                   │
│    - Fields: U (points), V (points)                                 │
│                                                                     │
│  Fides casts the address string back to adios2::core::IO*,          │
│  opens an inline reader, and reads U/V directly from memory.        │
└──────────────────────────┬──────────────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────────────────┐
│                  catalyst.py  (ParaView Pipeline)                    │
│                                                                     │
│  When run via Catalyst (not __main__):                               │
│    1. SetupRenderView()       → 3D render view with camera angles   │
│    2. SetupCatalystProducer() → TrivialProducer("fides")            │
│    3. SetupVisPipeline()      → Contour filter on V at [0.1–0.7]   │
│    4. SetupExtractor()        → PNG image writer                    │
│                                                                     │
│  catalyst_execute() called each step:                                │
│    - Updates pipeline                                               │
│    - Rescales color transfer function                               │
│    - Prints U/V value ranges                                        │
│    - Optionally streams to ParaView Live (CatalystLive enabled)     │
│                                                                     │
│  Outputs: output_{timestep:06d}.png (contour images of V field)     │
└─────────────────────────────────────────────────────────────────────┘
```

## Per-Step Data Flow

```
 BeginStep()                          EndStep()
     │                                    │
     ├─ InlineWriter->BeginStep()         ├─ ComputeDerivedVariables()
     │                                    ├─ InlineWriter->EndStep()
     │   ┌──── DoPutSync_() ────┐         ├─ CatalystExecute()
     │   │                      │         │     └─ catalyst_execute(conduit_node)
     │   │  hermes_->Put(...)   │         │           └─ Fides reads InlineIO
     │   │  InlineWriter->Put() │         │                 └─ ParaView pipeline runs
     │   │  db metadata insert  │         │                       └─ PNG / Live output
     │   └──────────────────────┘         ├─ delete hermes_->tag
     │                                    │
     ▼                                    ▼
```

## Zero-Copy Data Path

The key to zero-copy in-situ visualization is the inline engine's pointer-based storage and address passing:

```
Simulation buffer (values pointer)
       │
       │  Put(*inlineVar, values)   ← just stores the pointer
       ▼
  InlineWriter (ADIOS2 inline engine)
       │
       │  &InlineIO address passed as string
       ▼
  Fides (opens inline reader at that address, reads values pointer)
       │
       ▼
  Catalyst / ParaView Python script
```

**How it works:**
- The simulation's `values` pointer is passed directly to `InlineWriter->Put()` — no data copy occurs
- The inline engine records the pointer, not the data itself
- `CatalystExecute()` serializes the memory address of `InlineIO` as a string in the Conduit node
- Fides receives this address, casts it back to `adios2::core::IO*`, and opens an inline reader
- The reader returns the same `values` pointer, giving Fides direct access to simulation memory
- All data stays in-process — no serialization, no files, no network transfers

## Key Design Points

1. **Compile-time optional** — All Catalyst code is behind `#ifdef COEUS_HAVE_CATALYST`. Without Catalyst installed, the engine works normally with CTE/Hermes only.

2. **Zero-copy data path** — The ADIOS2 inline engine stores raw pointers, not copies. Fides reads the same memory the simulation wrote to. No serialization or disk I/O.

3. **Dual-write architecture** — Every `Put()` writes to both CTE/Hermes (for persistence/staging) and the InlineWriter (for in-situ visualization). The two paths are independent.

4. **Fides data model** — `gs-fides.json` describes how to interpret the ADIOS2 variables (U, V) as a VTK Cartesian grid with uniform spacing. This eliminates the need for a custom Catalyst adaptor.

5. **Activation via ADIOS2 XML** — Catalyst is only activated when both `Script` and `DataModel` parameters are present in the `adios2.xml` config. No code changes needed to enable/disable.

## Configuration Files

| File | Purpose |
|---|---|
| `setting.json` | Gray-Scott simulation parameters (grid size, time steps, etc.) |
| `adios2.xml` | ADIOS2 engine config — selects HermesEngine plugin, passes Catalyst params |
| `gs-fides.json` | Fides data model — maps ADIOS2 variables to VTK Cartesian grid |
| `catalyst.py` | ParaView Python script — defines visualization pipeline (contours of V) |

