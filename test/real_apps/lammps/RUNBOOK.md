# LAMMPS → COEUS hermes engine: build, run, and internals

End-to-end notes for the Vigil **LAMMPS** case (velocity-Verlet integration
failure) driven through the COEUS `hermes` plugin engine with a temperature
trigger. Covers building an ADIOS-enabled LAMMPS, the source we added to the
ADIOS dump, the engine-side `mean` trigger, and two ways to run it: standalone
and through the jarvis `coeus-gray-lammps` pipeline.

Verified on Ares, 2026-07-12 (single rank, `ares-comp-26`).

---

## 1. The physics and the derived quantity

Dilute Lennard-Jones fluid, timestep 10× too large (`dt*=0.05`): temperature
runs away from the `T*=0.75` set point and LAMMPS aborts with `ERROR: Lost
atoms` around step 6. The trigger signal is the **kinetic temperature**, built
from the velocity field — the velocity-field analog of Gray-Scott's
`variance(V)`:

| Form | Expression | Value | Used by |
|---|---|---|---|
| Exact temperature | `mean(multiply(magnitude(vx,vy,vz),magnitude(vx,vy,vz)))` | `<\|v\|²>` = **3·T\*** | (blocked, see §6) |
| Per-DOF variance | `variance(vx)` ≈ `<vx²>` = **T\*** | raw-field trigger | **the working path** |

`variance(vx)` at each step (single rank, matches offline BP5 exactly):

```
step   0      1      2      3      4      5
var  0.757  0.786  0.841  0.810  0.861  1.161
```

Baseline (step 0) = 0.757; at the last pre-crash step it reaches 1.16 = 1.53×
baseline, so `TriggerBaselineRatio=1.4` fires.

---

## 2. Build the ADIOS-enabled LAMMPS

The spack `lammps@20250722.3` is built `~adios` (no ADIOS dump). We build our
own from `~/software/lammps_bench/lammps` (LAMMPS "30 Mar 2026"):

```bash
spack load adios2-coeus@vigil openmpi@5.0.9
cd ~/software/lammps_bench/lammps
cmake -S cmake -B build \
  -D CMAKE_BUILD_TYPE=Release \
  -D PKG_ADIOS=ON \
  -D CMAKE_CXX_COMPILER=$(which mpicxx)
cmake --build build --parallel 16
```

Binary: `~/software/lammps_bench/lammps/build/lmp` (has `custom/adios` +
`atom/adios`). After editing the dump source below, rebuild is incremental
(`cmake --build build --parallel 16`, a few seconds).

---

## 3. Source we added

### 3a. LAMMPS `dump custom/adios` — `src/ADIOS/dump_custom_adios.cpp`

Stock LAMMPS writes every per-atom column into a **single anonymous 2-D matrix
variable** `atoms {natoms, ncols}` (column names are only a string attribute),
so ADIOS2 has no `vx` variable to trigger on. We changed the dump to:

1. **De-interleave named per-column variables.** In the once-only define block
   (after `varAtoms`), define a 1-D global variable per dump column (name
   sanitized to a valid ADIOS token). At write time, split the row-major `buf`
   into per-column buffers (kept alive in `internal->colData` for the deferred
   Put) and `Put` each.
2. **Optionally declare a derived temperature.** Gated behind env
   `COEUS_LAMMPS_DERIVED` (default OFF — see §6): when set and `vx/vy/vz` are
   dumped, define `derive/VarVx = variance(vx)` and `derive/AddVx = add(vx)`.

New members on `DumpCustomADIOSInternal`: `std::vector<adios2::Variable<double>>
colVars`, `std::vector<std::vector<double>> colData`, `bool haveTempDerived`.
Added `#include <cctype>`.

### 3b. Engine `mean` trigger — `src/hermes_engine.cc`, `include/coeus/HermesEngine.h`

Added `TriggerType=mean`: `EvaluateTrigger_` branches to
`ComputeGlobalBlockMean_(trigger_variable_)` (the N_b-weighted block-mean
pooling already used by the dissipation trigger) and reuses the whole
threshold / baseline-ratio / rising-edge / inspect-window / SST-gating path.
Config-parse and fired-log labels generalized to print `mean(...)` vs
`variance(...)`. *(Not exercised in the working run — see §6 — but compiled in.)*

---

## 4. Run A — standalone (no jarvis), single node

Fastest way to see the trigger fire. Needs the clio runtime + CTE pool up
(see `docs/BUILD_AND_RUN_GRAY_SCOTT.md` §2) and an `adios2_config.xml` binding
IO group `custom` to the hermes plugin.

```bash
spack load iowarp@main adios2-coeus@vigil openmpi@5.0.9
export LD_LIBRARY_PATH=~/coeus/iowarp/coeus-adapter/build/bin:$LD_LIBRARY_PATH
cd test/real_apps/lammps          # has adios2_config.xml + in.lj_explosion_hermes
mpirun -n 1 ~/software/lammps_bench/lammps/build/lmp -in in.lj_explosion_hermes
```

`adios2_config.xml` here uses the raw-field trigger (`TriggerVariable=vx`,
`TriggerType=variance`) — the path that works today.

---

## 5. Run B — jarvis pipeline `coeus-gray-lammps` (the tested path)

Pipeline stages: `runtime` (clio_runtime) → `cte_core` (clio_cte) →
`jarvis_coeus.lammps`. The lammps package
(`test/jarvis/jarvis_coeus/jarvis_coeus/lammps/pkg.py`) materializes
`input.lammps` + `adios2_config.xml` into `script_location` and launches
`lmp -in input.lammps` via mpirun.

```bash
# Environment (once per shell)
spack load adios2-coeus@vigil iowarp@main openmpi@5.0.9
export PATH=~/coeus/iowarp/coeus-adapter/build/bin/:$PATH
export LD_LIBRARY_PATH=~/coeus/iowarp/coeus-adapter/build/bin/:$LD_LIBRARY_PATH
cd ~/coeus/iowarp/coeus-adapter/test/jarvis/jarvis_coeus/pipelines
jarvis ppl env build          # snapshot the env into the pipeline (once)

# Configure the lammps stage (raw-field temperature trigger)
jarvis pkg configure coeus-gray-lammps.jarvis_coeus.lammps \
    engine=hermes trigger=true nprocs=1 ppn=1 \
    script_location=/mnt/common/hxu40/lammps-run \
    rho=0.5 box=8 t0=0.75 dt=0.05 steps=5 dump_every=1 \
    trigger_type=variance trigger_variable=vx trigger_sum_variable=none \
    trigger_threshold=0 trigger_baseline_ratio=1.4

# Run
jarvis ppl kill && jarvis ppl run
```

### Key package knobs (`pkg.py`)

| Config | Default | Meaning |
|---|---|---|
| `engine` | `hermes` | `bp4` (plain ADIOS) or `hermes` (plugin engine) |
| `script_location` | — | NFS-shared run dir (materialized files + `lammps.bp`) |
| `lmp_bin` | `.../lammps/build/lmp` | the +adios binary |
| `rho`,`box`,`t0`,`dt`,`steps`,`dump_every` | 0.5, 8, 0.75, 0.05, 5, 1 | LJ case params |
| `trigger` | `false` | enable the hermes-engine trigger |
| `trigger_type` | `variance` | `variance` (raw `vx`) or `mean` (derive/V2mean) |
| `trigger_variable` | `derive/VarVx` | set to `vx` for the raw-field path |
| `trigger_sum_variable` | `derive/AddVx` | `none` for the raw-field path |
| `trigger_threshold` | `0` | absolute threshold (0 disables) |
| `trigger_baseline_ratio` | `1.4` | fire at ratio × first-step baseline |
| `trigger_inspect_steps` | `3` | steps per fire |

### Expected output

```
Trigger FIRED at step 6: variance(vx) = 1.1605 (threshold 0, baseline 0.7568, ratio 1.4), streaming 3 step(s)
```

and a JSON-lines event in `<script_location>/lammps_trigger_log.jsonl`:

```json
{"event":"trigger_fired","step":6,"variable":"vx","stat":"variance","value":1.16054,"baseline":0.756768,"baseline_ratio":1.4,"inspect_steps":3}
```

---

## 6. Known issues / limitations

1. **Derived vs. raw trigger variable.** The ADIOS2 **derived** path
   (`derive/VarVx = variance(vx)`, opt-in `COEUS_LAMMPS_DERIVED` / jarvis
   `derived=true`) is the Gray-Scott-style route and works — see
   `DERIVED_QUANTITIES.md`. The alternative is the **raw-field** trigger
   (`trigger_variable=vx`), which computes the variance directly from the CTE
   blob via `ComputeGlobalVariance_` with no derived variables.

2. **Two-hostfile gotcha (Ares).** The app's mpirun uses the *global* jarvis
   hostfile (`~/server_list/server.list`); the runtime daemons use the
   *pipeline* hostfile (`/mnt/common/hxu40/jarvis-pipelines/coeus-gray-lammps/
   hostfile`). If they differ, LAMMPS lands on a node with no daemon →
   `ERROR: Could not initialize Chimaera`. We added `ares-comp-26` (the launch
   host, where mpirun places ranks by default) to the pipeline hostfile so app
   and runtime co-locate.

3. **Render side not wired.** Streaming flagged atoms to ParaView needs a Fides
   **particle** data model (the Gray-Scott uniform-grid `gs-fides.json` does
   not apply); `x y z` are already dumped for it. Enable via the commented
   Catalyst block in `adios2_config.xml`.

## 7. File inventory

| File | Purpose |
|---|---|
| `in.lj_explosion_hermes` | standalone LAMMPS input (raw-field trigger) |
| `adios2_config.xml` | standalone hermes-plugin config (IO group `custom`) |
| `RUNBOOK.md` | this file |
| `README.md` | shorter orientation |
| `../../jarvis/jarvis_coeus/jarvis_coeus/lammps/pkg.py` | jarvis package |
| `../../jarvis/jarvis_coeus/jarvis_coeus/lammps/config/{in.lammps,hermes_trigger.xml,hermes.xml,adios2.xml}` | jarvis templates |
| `~/software/lammps_bench/lammps/src/ADIOS/dump_custom_adios.cpp` | modified dump |
| `src/hermes_engine.cc`, `include/coeus/HermesEngine.h` | engine `mean` trigger |
