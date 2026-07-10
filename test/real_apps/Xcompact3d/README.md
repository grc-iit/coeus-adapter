# Xcompact3d TGV — ADIOS2 derived quantities + dissipation trigger

Single reference for the Xcompact3d "dark waste" case of the Vigil
`trigger-render-reason` pipeline: silent numerical dissipation on an
under-resolved Taylor-Green Vortex (TGV), detected in situ by a two-stage
Yellow/Red trigger inside the Coeus hermes engine.

This document consolidates and supersedes the former `DISSIPATION_TRIGGER.md`
and organizes the submodule docs
(`Incompact3d/derived_quantities.md`, `Incompact3d/derived_comparison.md`,
`Incompact3d/adios2_derived_metrics_guide.md` — kept as detailed dev logs).

**Components** (all in this directory):

| Piece | Where | Role |
|---|---|---|
| Xcompact3d (Incompact3d) | `Incompact3d/` (submodule) | TGV solver; `Case-TGV.f90:visu_tgv_init` registers the derived variables |
| Customized 2decomp-fft | `2decomp-fft/` (submodule) | I/O layer; `src/io.f90` provides the `decomp_2d_register_derived_*` helpers |
| ADIOS2 (vigil branch) | spack `adios2-coeus@vigil` | Derived-variable engine + custom `gradient`/`mean`/`spectrum` ops + `pow` fix |
| Coeus hermes engine | `src/hermes_engine.cc` (`TriggerType=dissipation`) | Pools derived block means, evaluates the trigger, gates the SST stream |
| Example engine config | `adios2-hermes-dissipation-trigger.xml` | Plugin/hermes + trigger parameters |

---

## 1. How to build

Verified 2026-07-09 on Ares (ares-comp-14): 2decomp-fft and Xcompact3d built
against spack `adios2-coeus@vigil` + `openmpi@5.0.9`.

### 1.1 Environment

```bash
source /home/hxu40/spack/share/spack/setup-env.sh
spack load adios2-coeus@vigil     # vigil ADIOS2: Derived+SST+Fortran+MPI, custom ops
spack load openmpi                # openmpi 5.0.9 (mpicc/mpicxx/mpif90)
export CC=mpicc CXX=mpicxx FC=mpif90
A=$(spack location -i adios2-coeus@vigil)
```

### 1.2 One-time ADIOS2 cmake-config patch

2decomp-fft calls `find_package(adios2)` twice, and the generated ADIOS2
config errors on the second call ("cannot create imported target
adios2::cxx11 … already exists"). Guard the deprecated block once per
install:

```bash
sed -i 's/}_CXX_FOUND)/}_CXX_FOUND AND NOT TARGET adios2::cxx11)/' \
    $A/lib/cmake/adios2/adios2-config-common.cmake
```

(Re-apply if the spack package is ever reinstalled.)

### 1.3 Build 2decomp-fft (customized), install to `opt/`

From this directory (`test/real_apps/Xcompact3d/`):

```bash
cmake -S 2decomp-fft -B 2decomp-fft/build -DCMAKE_BUILD_TYPE=Release \
  -DIO_BACKEND=adios2 -Dadios2_DIR=$A/lib/cmake/adios2 \
  -DCMAKE_INSTALL_PREFIX=$PWD/2decomp-fft/opt -DBUILD_TESTING=OFF
cmake --build 2decomp-fft/build -j8
cmake --install 2decomp-fft/build
```

### 1.4 Build Xcompact3d against it

```bash
cmake -S Incompact3d -B Incompact3d/build -DCMAKE_BUILD_TYPE=Release \
  -DIO_BACKEND=adios2 -Dadios2_DIR=$A/lib/cmake/adios2 \
  -Ddecomp2d_DIR=$PWD/2decomp-fft/opt/lib/decomp2d
cmake --build Incompact3d/build -j8
```

Sanity check — the binary must resolve the vigil ADIOS2:

```bash
ldd Incompact3d/build/bin/xcompact3d | grep adios2
# ...adios2-coeus-vigil-.../lib/libadios2_fortran_mpi.so.2.11 ...
```

Notes:
- `Incompact3d/build_single/` is a stale MPI-backend (`f95`) build tree from
  the original working copy — ignore it; the adios2 build lives in
  `Incompact3d/build/`.
- Any ADIOS2 2.11 can *read* the output BP5; the vigil build is only needed
  to *write* (it evaluates the derived expressions).

---

## 2. Native derived quantities in the TGV case

Everything starts from the velocity-gradient tensor, computed with
Xcompact3d's 6th-order compact derivatives (`derx/dery/derz`) at output time
(`Case-TGV.f90:595-618`):

| Field | Type | Definition |
|-------|------|------------|
| `ux, uy, uz`, `pp` | primary (solved) | velocity, pressure |
| `vort` | derived | vorticity magnitude \|∇×u\| (`Case-TGV.f90:620-624`) |
| `critq` | derived | Q-criterion, 2nd invariant of ∇u (`Case-TGV.f90:626-632`) |
| `B_x/y/z` | state (MHD only) | magnetic field (induction eq. or imposed) |
| `J_x/y/z` | derived (MHD only) | current density ∇×B/Rem, or −∇φ + u×B (`mhd.f90`) |

The native global scalars (E_k, enstrophy, eps, skewness, flatness, E(k))
land in `time_evol.dat`. Full formulas: `Incompact3d/derived_quantities.md`.

---

## 3. ADIOS2 derived-variable offload

The paper's metrics are recomputed **in situ by ADIOS2** at `EndStep`,
per writer block, before data leaves the node.

### 3.1 Two facts every expression must respect

1. **Reversed storage (z,y,x).** 2decomp writes physical x as the fastest
   (last) array axis; ADIOS2 `curl`/`gradient` hard-assume axis0=x. So `curl`
   is fed swapped — `curl(vz,vy,vx)` — and ∂/∂x is `gradient(u, 2)`.
   `magnitude`/`mean`/`spectrum` are axis-symmetric (no swap).
2. **Index-space differentiation.** ADIOS2 stencils divide by the index gap,
   not physical Δx: multiply curl results by `1/Δ`, enstrophy density by
   `0.5/Δ²`. Skewness/flatness are ratios — Δ cancels. **Isotropic grids
   only** (Δ=dx=dy=dz); stretched grids are not expressible.

The `decomp_2d_register_derived_*` helpers in `2decomp-fft/src/io.f90` bake
both in.

### 3.2 Custom ops in adios2-coeus@vigil

- **`gradient(f[,axis])`** — 2nd-order central partials (full ∂u_i/∂x_j;
  stock `curl` only gives antisymmetric mixes).
- **`mean(f)`** — reduces a field to one value per writer block (the block
  statistic the trigger pools).
- **`spectrum(ux,uy,uz)`** (alias `fft`) — shell-binned energy spectrum E(k),
  self-contained radix-2 FFT, power-of-two dims, Parseval-exact.
- **`pow` fix** — non-integer exponents (`pow(x,1.5)`) previously truncated
  to integer; required for skewness.

Expression language: function calls only (no infix `*`, `/`), no `E`-notation
literals (fixed-point).

### 3.3 What visu_tgv_init registers

```fortran
! densities (fields)
tke_density   = multiply(pow(magnitude(ux,uy,uz),2), 0.5)
enst_density  = multiply(pow(magnitude(curl(uz,uy,ux)),2), 0.5/dx^2)
! per-block scalars (trigger inputs)
tke_mean      = mean(tke_density expression)
enst_mean     = mean(enst_density expression)
! diagnostics
skewness_dudx = divide(mean(pow(gradient(ux,2),3)), pow(mean(pow(gradient(ux,2),2)),1.5))
flatness_dudx = divide(mean(pow(gradient(ux,2),4)), pow(mean(pow(gradient(ux,2),2)),2))
ek_spectrum   = spectrum(ux,uy,uz)
! MHD only
J_derived     = multiply(curl(Bz,By,Bx), -1/(dx*Rem))
```

### 3.4 Measured agreement vs native (32³ TGV Re=400; OTV Rem=50)

| metric | rel. error vs native 6th-order |
|--------|:---:|
| E_k (TKE) | 0.000 % (exact — no derivative) |
| enstrophy / eps = 2ν·enstrophy | 0.97 % |
| eps_num, nu_eff/nu (post) | ~1 % |
| skewness du/dx | 2.9 % |
| flatness du/dx | 0.14 % |
| E(k) spectrum | 1.3e-15 vs numpy.fft; Parseval exact |
| J_x/J_y/J_z (MHD) | corr ≥ 0.9996, interior L2 0.6–2.3 % |

The residuals are pure 2nd-vs-6th-order truncation (`sin(kΔ)/(kΔ)`).
Reader gotchas for `J_derived`: component order is **(Jz,Jy,Jx)** and memory
is **component-last** — reshape the flat buffer to `(nz,ny,nx,3)` regardless
of the advertised Shape. Details: `Incompact3d/derived_comparison.md`.

---

## 4. Dissipation trigger (hermes engine, `TriggerType=dissipation`)

### 4.1 Data flow

1. Xcompact3d writes `ux,uy,uz` (+ derived registrations) through the
   `solution-io` IO each output step; with the Plugin/hermes engine the
   blobs land in CTE.
2. `HermesEngine::ComputeDerivedVariables()` evaluates the registered
   expressions at `EndStep`; each rank's block reduces `tke_mean`/`enst_mean`
   to a single value in CTE.
3. `EvaluateDissipationTrigger_()` pools the block means exactly
   (N_b-weighted, one 2-double Allreduce each) and forms the paper metrics:

   ```
   eps_total = -(E_k(t) - E_k(t-Δt)) / Δt        Δt = TriggerOutputDt
   eps_phys  = 2 · nu · <enstrophy>              (exact for periodic flow)
   eps_frac  = (eps_total - eps_phys) / eps_total   # numerical share
   nu_ratio  = eps_total / eps_phys                 # nu_eff / nu
   ```

4. **Yellow** (`eps_frac ≥ TriggerYellowFraction` or
   `nu_ratio ≥ TriggerYellowNuRatio`): log-only — `trigger_yellow` JSONL
   event on the rising edge (hook for the asynchronous LLM text monitor).
5. **Red** (either red threshold): opens the SST inspect window —
   `TriggerInspectSteps` steps re-Put from CTE with the `vigil/trigger_*`
   scalars (`vigil/trigger_stat` carries `eps_frac`); unflagged steps ship
   nothing. Fire-once unless `TriggerRefire=true`.
6. Optional `TriggerMetricsLogFile`: rank 0 appends every step's
   `{step, ke, enstrophy, eps_total, eps_phys, eps_frac, nu_ratio}` —
   the eval2-style timeline data.

### 4.2 Engine parameters (ADIOS2 XML)

Complete example: `adios2-hermes-dissipation-trigger.xml`.

| Parameter | Meaning | Default |
|---|---|---|
| `TriggerType` | `dissipation` selects this trigger (`variance` = gray-scott path) | `variance` |
| `TriggerKEVariable` | derived block-mean TKE (`tke_mean`) | required |
| `TriggerEnstrophyVariable` | derived block-mean enstrophy (`enst_mean`) | required |
| `TriggerNu` | kinematic viscosity (1/Re) | required |
| `TriggerOutputDt` | sim time between output steps (`dt*ioutput` from input.i3d) | required |
| `TriggerYellowFraction` / `TriggerRedFraction` | `eps_frac` thresholds | 0.05 / 0.15 |
| `TriggerYellowNuRatio` / `TriggerRedNuRatio` | `nu_ratio` thresholds | 1.05 / 1.2 |
| `TriggerInspectSteps`, `TriggerRefire`, `TriggerLogFile`, `TriggerMetricsLogFile` | shared trigger knobs | 3 / false / trigger_log.jsonl / off |

Without `CatalystStream`/`Script`/`DataModel` the trigger is log-only (no
SST gating).

### 4.3 Semantics & guards

- First evaluated step has no `dE_k/dt` — no escalation before the second
  output.
- If TKE is not decaying (`eps_total ≤ 0`, early TGV phase), `eps_frac` and
  `nu_ratio` are forced to 0 — no spurious fires while energy is flat.
- `eps_frac` clamps at 0 from below (over-resolved flow can give
  `eps_total < eps_phys` by the ~1% curl bias — that is not numerical
  dissipation).
- Per-run-unique actions (logs, scalar Puts) are guarded on the MPI rank,
  not the consensus rank (consensus ranks keep incrementing across app runs
  while the runtime stays up).

### 4.4 Accuracy caveats (inherited from the derived ops)

- `curl`/`gradient` differentiate per writer block with clamped edges →
  block-seam bias under MPI decomposition; the ~1% enstrophy bias is from
  single-rank validation. The 5%/15% thresholds have ample margin.
- The `0.5/Δ²` enstrophy scale assumes an **isotropic** grid.
- `ek_spectrum` is per-block: a global spectrum only with a single writer —
  do not trigger on it in decomposed runs.
- `skewness_dudx`/`flatness_dudx` are per-block ratio statistics, not
  exactly poolable — diagnostics only, not trigger inputs.

---

## 5. How to run

### 5.1 BP5 validation mode (derived scalars, no trigger)

Needs an `adios2_config.xml` with a **BP5** engine in the run dir:

```bash
cd Incompact3d/runs/tgv_scalars
mpirun -n 1 ../../build/bin/xcompact3d input.i3d   # 1 rank => global reductions
```

Writes `data.bp5/` (derived variables) and `time_evol.dat` (native scalars).
Compare with `compare_newops.py` / `check_spectrum.py`; MHD J with
`runs/otv3d_cmp/compare_derived.py`.

### 5.2 Trigger mode (Plugin/hermes engine)

Place `adios2-hermes-dissipation-trigger.xml` as the run dir's
`adios2_config.xml`, set per run:

- `TriggerNu` = 1/Re (e.g. Re=5000 → 0.0002)
- `TriggerOutputDt` = `dt * ioutput` from `input.i3d`

and run under the clio runtime (Chimaera + CTE up, `LD_LIBRARY_PATH`
including the coeus-adapter `build/bin` for `libhermes_engine.so`). With
`CatalystStream` set, an SST reader must connect (rendezvous) and receives
only the Red-window steps.

### 5.3 Running through jarvis (pipeline `coeus-xcompact3d`)

The `jarvis_coeus.Incompact3d` package materializes all of the above.
Pipeline = `clio_runtime` → `clio_cte` → `Incompact3d`; knobs (see
`test/jarvis/.../Incompact3d/pkg.py`): `catalyst_stream` (e.g. `tgv.bp`;
empty = no SST), `sst_data_transport`, `sst_queue_full_policy`,
`trigger=true` + `trigger_*` (thresholds, inspect steps, log files;
`trigger_nu`/`trigger_output_dt` auto-derive from the `re`/`dt`/
`io_frequency` knobs), and `nx/ny/nz/re/dt` for the tgv benchmark grid.

```bash
jarvis ppl kill; jarvis ppl run        # writer blocks in Open until a reader connects
# on another node:
spack load paraview
pvbatch test/real_apps/Xcompact3d/catalyst/tgv-pipeline.py \
    -j test/real_apps/Xcompact3d/catalyst/tgv-fides.json \
    -b <output_location>/tgv.bp --staging --num-steps N
# ungated: N = number of snapshots; gated: N = trigger_inspect_steps
```

The contact file `tgv.bp.sst` appears in `output_location` (NFS-shared);
remove stale ones before a rerun. jarvis parser gotchas: `configure`
silently ignores `x=false` and any value equal to the menu default — edit
`jarvis-pipelines/pipelines/coeus-xcompact3d/pipeline.yaml`, then run
`jarvis pkg configure coeus-xcompact3d.jarvis_coeus.Incompact3d` with no
args to re-materialize. `jarvis ppl kill` does not kill mpirun app ranks
(`pkill xcompact3d` on all nodes before a rerun).

### 5.4 Verified experiments (2026-07-09/10, Ares, commits 41577ab/ded9a99)

**Ungated SST, 385³ Re=1600, 64 ranks / 4 nodes:** rendezvous, snapshots
every 20 steps ship over SST, pvbatch rendered frames with exact physics
(vort max 1.9946 vs the analytic TGV initial max 2). Throughput caveat:
one pvbatch reader pulls a 5-field 385³ step (~2.3 GB) in ~25 min, and with
`QueueLimit=1` the writer stalls at EndStep while a step is being read —
trim the fides JSON fields or investigate the SST dataplane before scaling
ungated runs (gray-scott 512³ ×2 fields moved ~2 GB in ~5 s on the same
fabric).

**Dissipation trigger, 65³ Re=5000 dt=0.005 ioutput=20 (eval2
under-resolved case), single rank:** transient Yellow at t=0.2 (log-only);
guards hold `eps_frac=0` through the whole non-decaying phase (36 quiet
outputs, no spurious fire); Yellow t=3.8 (eps_frac 0.124); **Red t=3.9:
eps_frac=0.355, nu_ratio=1.55** (cascade onset; eval2's native timeline Red
≈ t=4.9); exactly `trigger_inspect_steps=3` steps shipped; the reader —
idle until the fire — rendered vort max 13.2→15.7→19.9 and exited; the
run continued log-only to t=8 (eps_frac 0.73, nu_ratio 3.7 at the end).
Logs of record: `logs/tgv_trigger_{writer,sst_consumer}.log`,
`logs/tgv_trigger_{log,metrics}.jsonl`.

### 5.5 Known issues

- **[FIXED 2026-07-10] Derived block means corrupt under MPI
  decomposition** (the trigger's inputs): `tke_mean`/`enst_mean` were exact
  at 1 rank but garbage at 25/64 ranks (pooled E_k oscillated ±12%,
  enstrophy ~60× inflated → spurious immediate Red). Two stacked root
  causes, found by dumping per-rank blobs from the engine
  (`COEUS_DERIVED_DEBUG=1`):
  1. *CTE tag collisions*: the `rankConsensus` pool keeps one atomic
     counter per node, so `GetRank(PoolQuery::Local())` handed out
     per-node ranks and the `step_N_rankR` tags collided across nodes —
     ranks read/overwrote each other's blobs. Fix (`hermes_engine.cc`):
     only MPI rank 0 draws a consensus value per run (the across-runs
     uniquifier) and broadcasts it; every rank uses
     `base*1000000 + mpi_rank`.
  2. *Unreversed dims*: 2decomp registered ADIOS2 variables with
     Fortran-order (x,y,z) dims, but the ADIOS2 Fortran bindings do not
     reverse them — metadata claimed axis0=x while x is the fastest axis
     in memory. Invisible on cubes (all prior validation), it transposes
     the stride map on pencil blocks, so `curl`/`gradient` differentiate
     across wrong strides. Fix (`2decomp-fft/src/io.f90`): register
     shape/start/count reversed (z,y,x) = true C-order; the established
     axis convention (stride-1 axis 2 = physical x) is unchanged.

  Verified at 25 ranks / 4 nodes after the fix: E_k 0.12681→0.126723
  (smooth), enstrophy 0.378→0.465 (physical, matches the single-rank
  trajectory), no spurious fire.
- **`ek_spectrum` aborts the run on non-power-of-two dims** ("spectrum
  requires power-of-two dimensions" at EndStep). Guarded in
  `Case-TGV.f90:visu_tgv_init` (registers only for single-rank,
  power-of-two grids) — the guard lives in the Incompact3d submodule tree.
- **Restart checkpoints through hermes `restart-io` fail** ("Writing
  restart - validation failed!" → MPI_ABORT). Worked around in the jarvis
  tgv benchmark with `icheckpoint = 1000000`.

### 5.6 Status

- [x] Engine `TriggerType=dissipation` implemented and compiling
      (`EvaluateDissipationTrigger_`, `ComputeGlobalBlockMean_`), sharing the
      gate/window/SST machinery with the variance trigger.
- [x] `Case-TGV.f90` + 2decomp registrations verified identical to the
      original working tree.
- [x] 2decomp-fft + Xcompact3d rebuilt against spack `adios2-coeus@vigil`
      (2026-07-09, this guide's Section 1).
- [x] TGV end-to-end through the hermes engine: ungated SST at 385³/64
      ranks and trigger-gated fire at 65³ single rank (Section 5.4).
- [ ] Fix multi-rank derived block means (vigil-ADIOS2 `ApplyExpression`,
      Section 5.5) — blocker for decomposed trigger runs.
- [ ] Bridge: ParaView pipeline for Q-criterion rendering of flagged steps.

---

## 6. File index

| File | Role |
|------|------|
| `adios2-hermes-dissipation-trigger.xml` | example engine config (trigger parameters) |
| `catalyst/tgv-pipeline.py` | pvbatch SST consumer (renders a vort slice per received step) |
| `catalyst/tgv-fides.json` | Fides data model for the TGV stream (no `step_information` — the writer emits no step variable) |
| `../../jarvis/jarvis_coeus/jarvis_coeus/Incompact3d/` | jarvis package: SST + trigger knobs, `config/hermes_{sst,trigger}.xml` |
| `2decomp-fft/src/io.f90` | `decomp_2d_register_derived_*` helpers |
| `Incompact3d/src/Case-TGV.f90` | `visu_tgv_init` registrations, native derived fields |
| `Incompact3d/adios2_derived_metrics_guide.md` | full offload guide (ops, API, measurements) |
| `Incompact3d/derived_comparison.md` | dev log: native vs ADIOS2 head-to-heads |
| `Incompact3d/derived_quantities.md` | native TGV derived-field formulas |
| `Incompact3d/runs/` | validation cases (`tgv_scalars`, `otv3d_cmp`, eval scenarios) |
| `../../src/hermes_engine.cc` | trigger implementation (`TriggerType=dissipation`) |
