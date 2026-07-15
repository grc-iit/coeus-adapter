# LAMMPS through the hermes engine — kinetic-temperature trigger

Single reference for the **LAMMPS** case of the Vigil
[Trigger-Render-Reason pipeline](../../../docs/TRIGGER_RENDER_REASON_PIPELINE.md):
a velocity-Verlet integration failure (timestep 10× too large) detected in situ
by a **temperature trigger** inside the Coeus `hermes` engine, which gates the
SST render stream. It is the `TriggerType=mean` / velocity-field analog of the
[Gray-Scott variance case](../../../docs/BUILD_AND_RUN_GRAY_SCOTT.md).

This document consolidates and supersedes `RUNBOOK.md` and
`DERIVED_QUANTITIES.md` (kept alongside as detailed dev logs).

**Components** (all in this directory unless noted):

| Piece | Where | Role |
|---|---|---|
| LAMMPS (+adios build) | `~/software/lammps_bench/lammps/build/lmp` | `dump custom/adios` writes atoms through the engine |
| Modified ADIOS dump | `src/ADIOS/dump_custom_adios.cpp` (in the LAMMPS tree) | de-interleaves named per-column vars; optional derived temperature |
| ADIOS2 (vigil branch) | spack `adios2-coeus@vigil` | derived-variable engine (`variance`, `mean`, `add`) |
| Coeus hermes engine | `../../../src/hermes_engine.cc` (`TriggerType=mean`) | pools the block statistic, evaluates the trigger, gates SST |
| Standalone config | `adios2_config.xml` | Plugin/hermes engine + trigger parameters (IO group `custom`) |
| Standalone input | `in.lj_explosion_hermes` | dilute LJ, `dt*=0.05` explosion; dumps `id x y z vx vy vz` |

---

## 1. The case and the trigger signal

Dilute Lennard-Jones fluid with a timestep 10× too large (`dt*=0.05`): the
kinetic temperature runs away from the `T*=0.75` set point and LAMMPS aborts
with `ERROR: Lost atoms` around step 6. The trigger watches the **kinetic
temperature**, built from the velocity field — the velocity-field analog of
Gray-Scott's `variance(V)`.

The temperature can be expressed three equivalent ways; all are supported, and
the two `variance` forms are the verified working paths:

| Form | Expression | Value | Status |
|---|---|---|---|
| **raw-field variance** | `variance(vx)` — engine reads the `vx` CTE blob directly (`ComputeGlobalVariance_`) | ≈ `T*` | ✅ working (verified 2026-07-12) |
| **derived variance** | `derive/VarVx = variance(vx)` + `derive/AddVx = add(vx)`, pooled by `ComputeGlobalVarianceDerived_` | ≈ `T*` | ✅ working — Gray-Scott path, **engine unchanged** |
| **mean of \|v\|²** | `derive/V2mean = mean(multiply(magnitude(vx,vy,vz),…))` = `3·T*` | `3·T*` | engine `TriggerType=mean` compiled in (see `DERIVED_QUANTITIES.md`) |

`variance(vx)` per step (single rank, matches offline BP5 exactly):

```
step   0      1      2      3      4      5
var  0.757  0.786  0.841  0.810  0.861  1.161
```

Baseline (step 0) = 0.757; the last pre-crash step reaches 1.16 = 1.53× baseline,
so `TriggerBaselineRatio=1.4` fires at step 6. `derive/V2mean` is `3·T*`, so its
thresholds are in those units (divide by 3 for `T*`).

---

## 2. Build the ADIOS-enabled LAMMPS

The spack `lammps` is built `~adios` (no ADIOS dump). Build your own from
`~/software/lammps_bench/lammps` (LAMMPS "30 Mar 2026"):

```bash
spack load adios2-coeus@vigil openmpi@5.0.9
cd ~/software/lammps_bench/lammps
cmake -S cmake -B build -D CMAKE_BUILD_TYPE=Release -D PKG_ADIOS=ON \
      -D CMAKE_CXX_COMPILER=$(which mpicxx)
cmake --build build --parallel 16
```

Binary: `~/software/lammps_bench/lammps/build/lmp` (has `custom/adios` +
`atom/adios`). After editing the dump source, rebuild is incremental.

---

## 3. Source changes (LAMMPS-side only; engine untouched for the variance paths)

Stock LAMMPS writes every per-atom column into a single anonymous 2-D matrix
variable `atoms {natoms, ncols}`, so ADIOS2 has no `vx` variable to trigger on.
The modified `dump_custom_adios.cpp`:

1. **De-interleaves named per-column variables**, each defined **the way
   Gray-Scott defines a field** — global shape, local offset (`MPI_Scan`), local
   count, all at *define* time:
   ```cpp
   const size_t nGlobal = atom->natoms;
   bigint nlocal = atom->nlocal, off;
   MPI_Scan(&nlocal, &off, 1, MPI_LMP_BIGINT, MPI_SUM, world);  off -= nlocal;
   DefineVariable<double>(vname, {nGlobal}, {off}, {nlocal});   // was {1},{0},{1}
   ```
   Defining columns with a placeholder `{1}` shape was the original abort cause:
   a derived variable captures its source dims at `DefineDerivedVariable` time,
   so stale dims made `ApplyExpression` walk off the block. See
   `DERIVED_QUANTITIES.md`.
2. **Optionally declares the derived temperature** (`derive/VarVx`/`derive/AddVx`,
   or `derive/V2mean`), gated behind env `COEUS_LAMMPS_DERIVED` / jarvis knob
   `derived=true`.

Engine side: `TriggerType=mean` was added to `hermes_engine.cc` (branches to the
`ComputeGlobalBlockMean_` pooling shared with the dissipation trigger). The
`variance` paths need **no** engine change.

---

## 4. Run

Prerequisites for both: the clio runtime + CTE pool up (see
[BUILD_AND_RUN_GRAY_SCOTT.md §2](../../../docs/BUILD_AND_RUN_GRAY_SCOTT.md)), and
`libhermes_engine.so` on `LD_LIBRARY_PATH`.

### 4.1 Standalone (no jarvis), single node

```bash
spack load iowarp@main adios2-coeus@vigil openmpi@5.0.9
export LD_LIBRARY_PATH=~/coeus/iowarp/coeus-adapter/build/bin:$LD_LIBRARY_PATH
cd test/real_apps/lammps          # adios2_config.xml + in.lj_explosion_hermes
mpirun -n 1 ~/software/lammps_bench/lammps/build/lmp -in in.lj_explosion_hermes
```

`adios2_config.xml` here uses the raw-field trigger (`TriggerVariable=vx`,
`TriggerType=variance`).

### 4.2 jarvis pipeline `coeus-gray-lammps` (the tested path)

Stages: `runtime` (clio_runtime) → `cte_core` (clio_cte) →
`jarvis_coeus.lammps`.

```bash
spack load adios2-coeus@vigil iowarp@main openmpi@5.0.9
export PATH=~/coeus/iowarp/coeus-adapter/build/bin/:$PATH
export LD_LIBRARY_PATH=~/coeus/iowarp/coeus-adapter/build/bin/:$LD_LIBRARY_PATH
cd ~/coeus/iowarp/coeus-adapter/test/jarvis/jarvis_coeus/pipelines
jarvis ppl env build                              # snapshot env (once)

jarvis pkg configure coeus-gray-lammps.jarvis_coeus.lammps \
    engine=hermes trigger=true nprocs=1 ppn=1 \
    script_location=/mnt/common/hxu40/lammps-run \
    rho=0.5 box=8 t0=0.75 dt=0.05 steps=5 dump_every=1 \
    trigger_type=variance trigger_variable=vx trigger_sum_variable=none \
    trigger_threshold=0 trigger_baseline_ratio=1.4
jarvis ppl print | grep -E "trigger|engine"       # VERIFY before running
jarvis ppl kill && jarvis ppl run
```

For the derived path instead, set `derived=true trigger_variable=derive/VarVx
trigger_sum_variable=derive/AddVx` (see `DERIVED_QUANTITIES.md`).

**Key package knobs** (`../../jarvis/jarvis_coeus/jarvis_coeus/lammps/pkg.py`):

| Config | Default | Meaning |
|---|---|---|
| `engine` | `hermes` | `bp4` (plain ADIOS) or `hermes` (plugin engine) |
| `script_location` | — | NFS-shared run dir (materialized files + `lammps.bp`) |
| `trigger` | `false` | enable the hermes-engine trigger |
| `trigger_type` | `variance` | `variance` (raw `vx`) or `mean` (`derive/V2mean`) |
| `trigger_variable` | `derive/VarVx` | set to `vx` for the raw-field path |
| `trigger_sum_variable` | `derive/AddVx` | `none` for the raw-field path |
| `trigger_threshold` | `0` | absolute threshold (0 disables) |
| `trigger_baseline_ratio` | `1.4` | fire at ratio × first-step baseline |
| `trigger_inspect_steps` | `3` | output steps shipped per fire |

**Expected output:**

```
Trigger FIRED at step 6: variance(vx) = 1.1605 (threshold 0, baseline 0.7568, ratio 1.4), streaming 3 step(s)
```

plus a JSON-lines event in `<script_location>/lammps_trigger_log.jsonl`:

```json
{"event":"trigger_fired","step":6,"variable":"vx","stat":"variance","value":1.16054,"baseline":0.756768,"baseline_ratio":1.4,"inspect_steps":3}
```

---

## 5. Render side (not yet wired)

Streaming flagged atoms to ParaView needs a Fides **particle** data model (the
Gray-Scott uniform-grid `gs-fides.json` does not apply); `x y z` are already
dumped for it. Enable via the commented Catalyst block in `adios2_config.xml`
(`DataModel` + `Script` + `CatalystStream`). Multi-rank runs pool correctly (the
statistic is N_b-weighted per writer block).

## 6. Known issues

- **Two-hostfile gotcha (Ares).** The app's mpirun uses the *global* jarvis
  hostfile (`~/server_list/server.list`); the runtime daemons use the *pipeline*
  hostfile. If they differ, LAMMPS lands on a node with no daemon →
  `ERROR: Could not initialize Chimaera`. Co-locate app and runtime (add the
  launch host to the pipeline hostfile). Same root cause as the Gray-Scott
  two-hostfile note.
- **Temperature fires the explosion** (`dt*=0.05`). The subtle dense drift
  (`rho*=0.8442, dt*=0.02`) injects energy into *potential* energy while `T*`
  stays ~0.76 — that sub-case needs a per-atom PE derived signal
  (`compute pe/atom`), not temperature.

## 7. File index

| File | Role |
|---|---|
| `README.md` | this file — authoritative entry point |
| `RUNBOOK.md` | dev log: build/run/internals, verified 2026-07-12 |
| `DERIVED_QUANTITIES.md` | dev log: the derived `variance(vx)` path + the define-time-dims fix |
| `adios2_config.xml` | standalone hermes-plugin config (IO group `custom`) |
| `in.lj_explosion_hermes` | standalone LAMMPS input (raw-field trigger) |
| `../../jarvis/jarvis_coeus/jarvis_coeus/lammps/` | jarvis package (`pkg.py`, `config/`) |
| `~/software/lammps_bench/lammps/src/ADIOS/dump_custom_adios.cpp` | modified dump (LAMMPS tree) |
| `../../../src/hermes_engine.cc` | engine `mean` trigger (`TriggerType=mean`) |
