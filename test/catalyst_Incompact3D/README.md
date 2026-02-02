For installation, please refer to jarvis Incompact3D.

This directory contains the Fides data model and Catalyst pipeline for Incompact3d (Xcompact3d) in situ and post hoc visualization with ADIOS2.

## Application data (from Incompact3d `visu.f90`)

- **Core fields**: `ux`, `uy`, `uz` (velocity), `pp` (pressure) — written by all runs.
- **Optional**: `rho` (ilmn), `phi01`, `phi02`… (scalars), `warp` (istret); case-specific: `vort`, `critq` (e.g. TGV).

Grid: uniform Cartesian (ImageData); dimensions and spacing come from the run (Fides uses `variable_dimensions` from `ux`).

## Files

| File | Purpose |
|------|--------|
| **fide.json** | Fides schema for runs that write `vort` and `critq` (e.g. TGV). |
| **fide-minimal.json** | Fides schema with only `ux`, `uy`, `uz`, `pp` — use for Cavity, Channel, etc. |
| **pipeline.py** | Catalyst script: velocity magnitude, contours on `vort` / `pp` / `critq`; supports in situ and post hoc. |
| **catalyst.py** | Alternate pipeline (velocity magnitude / vorticity). |
| **incompact3D.json** | Legacy Fides schema (use fide.json or fide-minimal.json). |

ADIOS config (`adios2.xml`) should point `DataModel` to `fide.json` or `fide-minimal.json` and `Script` to `pipeline.py` (or `catalyst.py`).

## Paraview installation

```
spack install paraview@5.13.3 +adios2^python + qt +fides +mpi +libcatalyst +python ^py-mpi4py ^python-venv
```

The plugin uses the ADIOS inline engine to pass data to ParaView’s Fides reader and Catalyst to run the pipeline script.

## Environment setup

```
export ADIOS2_PLUGIN_PATH=/path/to/adios2-build/lib
export CATALYST_IMPLEMENTATION_NAME=paraview
export CATALYST_IMPLEMENTATION_PATHS=/path/to/paraview-build/lib/catalyst
```

## Run in situ

```
mpirun -n 4 xcompact3D
```

## Post hoc (read BP file)

```
python pipeline.py -j fide-minimal.json -b /path/to/data/snap.bp
python pipeline.py -j fide.json -b /path/to/tgv.bp -f vort
```