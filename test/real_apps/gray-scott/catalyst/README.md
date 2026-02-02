# Catalyst setup for Gray-Scott

Catalyst in situ visualization for Gray-Scott requires two pieces:

## 1. Fides data model: `fide.json` / `gs-fides.json`

- **`fide.json`** — Fides schema used by tools that expect this filename (same content as `gs-fides.json`).
- **`gs-fides.json`** — Fides data model used by the ADIOS inline plugin (see `DataModel` in `adios2-inline-plugin.xml`).

Both describe the **application data types and layout**:

| Item | Description |
|------|-------------|
| **Output type** | `VTK-Cartesian-grid` (uniform structured grid) |
| **Data source** | Single source named `"source"`, `filename_mode`: `"input"` |
| **Step** | From variable `step` |
| **Coordinates** | Uniform point coordinates; dimensions from variable `U`; origin `[0,0,0]`, spacing `[0.1,0.1,0.1]` |
| **Cell set** | Structured; dimensions from variable `U` |
| **Fields** | **U**, **V** — point data, double arrays from ADIOS variables `U` and `V` |

So the **data flow** from the app is: ADIOS variables `U`, `V`, `step` → Fides → VTK Cartesian grid with point arrays `U` and `V`.

## 2. Pipeline script: `gs-pipeline.py`

- **`gs-pipeline.py`** — ParaView Catalyst script that defines the **visualization pipeline and data flow**:

  - **Catalyst mode** (in situ): `TrivialProducer` named `'fides'` receives data from the Fides/ADIOS plugin; no file path.
  - **Post hoc mode**: `FidesJSONReader` reads a BP file using the Fides JSON (`-j`/`--json_filename`); optional SST engine via `--staging`.
  - **Pipeline**: Show uniform grid, contour on **V** (isosurfaces 0.1, 0.3, 0.5, 0.7), color by **V**, optional PNG extractor.
  - **Data flow**: Producer/reader → `SetupVisPipeline` (display + contour) → `catalyst_execute` updates each step and reports U/V ranges.

The ADIOS config references these in `adios2-inline-plugin.xml`:

- `DataModel`: `catalyst/gs-fides.json` (or `catalyst/fide.json` if you point the config there)
- `Script`: `catalyst/gs-pipeline.py`
