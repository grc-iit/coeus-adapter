# ADIOS2 derived quantities for LAMMPS - the Gray-Scott way (LAMMPS-side only)

Goal (paper design): the LAMMPS trigger uses **ADIOS2 derived variables** pooled
by the engine, exactly like Gray-Scott - `derive/VarVx = variance(vx)` +
`derive/AddVx = add(vx)`, combined by the existing
`HermesEngine::ComputeGlobalVarianceDerived_`. **No changes to the shared engine
code (`hermes_engine.cc`)** - the fix is entirely in the LAMMPS
`dump_custom_adios.cpp`.

Status (2026-07-12): **WORKING.** `Trigger FIRED ... variance(derive/VarVx) =
1.16054`, via the same engine path Gray-Scott uses, on the unmodified engine.

## 1. Root cause of the original abort

An ADIOS2 derived variable **captures its source dimensions at
`DefineDerivedVariable` time**. The LAMMPS dump defined its velocity columns
with a **placeholder shape `{1}`** (natoms not final in `init_style`) and only
`SetShape`/`SetSelection`'d them at write, so the derived expression kept stale
dims and `ApplyExpression` walked off the real block. It is **not** "ADIOS2
can't do 1-D." Gray-Scott never hits it because it defines its field `V` with
its true shape and count up front.

## 2. The fix (LAMMPS `dump_custom_adios.cpp`)

Define each velocity column **the way Gray-Scott defines a field**: global
shape, local offset, local count - all at define time, using the real
`atom->natoms` / `atom->nlocal` (+ an `MPI_Scan` for the offset):

```cpp
const size_t nGlobal = atom->natoms;
bigint nlocal = atom->nlocal, off;
MPI_Scan(&nlocal, &off, 1, MPI_LMP_BIGINT, MPI_SUM, world);  off -= nlocal;
DefineVariable<double>(vname, {nGlobal}, {off}, {nlocal});   // was {1},{0},{1}
```

The derived declaration is gray-scott-shaped: `derive/VarVx = variance(vx)`,
`derive/AddVx = add(vx)` (opt-in via env `COEUS_LAMMPS_DERIVED`, jarvis knob
`derived=true`). With correct define-time dims the engine's existing
`ComputeDerivedVariables` computes the per-block variance and
`ComputeGlobalVarianceDerived_` produces the trigger statistic - no engine
change.

## 3. Run the derived demo

```bash
spack load adios2-coeus@vigil iowarp@main openmpi@5.0.9
export PATH=~/coeus/iowarp/coeus-adapter/build/bin/:$PATH
export LD_LIBRARY_PATH=~/coeus/iowarp/coeus-adapter/build/bin/:$LD_LIBRARY_PATH
cd ~/coeus/iowarp/coeus-adapter/test/jarvis/jarvis_coeus/pipelines
jarvis pkg configure coeus-gray-lammps.jarvis_coeus.lammps \
    engine=hermes trigger=true derived=true nprocs=1 ppn=1 \
    script_location=/mnt/common/hxu40/lammps-run steps=5 \
    trigger_type=variance trigger_variable=derive/VarVx \
    trigger_sum_variable=derive/AddVx trigger_baseline_ratio=1.4
jarvis ppl kill && jarvis ppl run
# -> Trigger FIRED at step 6: variance(derive/VarVx) = 1.1605
```

`derived_debug=true` sets `COEUS_DERIVED_DEBUG` to log per-block derived
shape/count/blob diagnostics from the engine.

## 4. Files (LAMMPS-side only; engine untouched)

| File | Change |
|---|---|
| `~/software/lammps_bench/lammps/src/ADIOS/dump_custom_adios.cpp` | named columns defined gray-scott-style (global shape / local offset+count); `derive/VarVx`, `derive/AddVx` opt-in via `COEUS_LAMMPS_DERIVED` |
| `.../jarvis_coeus/lammps/pkg.py` | `derived` / `derived_debug` knobs |
| `src/hermes_engine.cc` | **unchanged** (committed state) |
