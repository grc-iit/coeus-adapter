# Gray-Scott `variance` Derived-Variable Trigger — Implementation & Empirical Findings

**Date:** 2026-07-07
**Context:** Vigil `trigger-render-reason` pipeline — Gray-Scott regime-identification case
**Components:** ADIOS2 derived-variable engine (`../ADIOS2/`) + gray-scott writer (`simulation/writer.cpp`)

---

## 1. Summary

The `variance(x)` derived-variable operator is **fully implemented, registered, built,
and installed** in the local ADIOS2 tree, and the gray-scott writer already emits it as
`derive/VarV` (and `derive/VarU`). An end-to-end `L=64` run confirms the operator produces
the intended **statistical trigger signal**: the variance of the `V` field rises ~30×
during pattern formation, peaks sharply, then collapses three orders of magnitude to a flat
steady-state plateau. This spike is exactly the perturbation → reactive-pattern transition
that Vigil's Gray-Scott trigger keys on.

One important semantic caveat was confirmed empirically: `variance` is a **per-writer-block
(per-MPI-rank) statistic**, not a global variance. A correct global variance must be pooled
across blocks on the reader/trigger side.

---

## 2. Where the operator lives (ADIOS2 side)

All changes are **uncommitted local modifications** on top of ADIOS2 HEAD `71c8f73a9`
(*"Add derived quantities: GRADIENT, MEAN, SPECTRUM operators"*). `variance` was added on
top of `mean` in the same style. Files touched:

| File | Change |
| ---- | ------ |
| `source/adios2/toolkit/derived/Expression.h` | `OP_VARIANCE` added to the `ExpressionOperator` enum |
| `source/adios2/toolkit/derived/Expression.cpp` | Registered in `op_property` (`"VARIANCE"`), `string_to_op` (`"VARIANCE"` **and** `"VAR"`), and the `OpFunctions` dispatch table → `{VarianceFunc, VarianceDimsFunc, FloatTypeFunc}` |
| `source/adios2/toolkit/derived/Function.h` | Declarations for `VarianceFunc` and `VarianceDimsFunc` |
| `source/adios2/toolkit/derived/Function.cpp` | Implementations of `VarianceFunc` (compute) and `VarianceDimsFunc` (output dims) |
| `source/adios2/toolkit/derived/DERIVED_QUANTITIES.md` | Docs |

### Parser requires no changes
The grammar rule `IDENTIFIER "(" list ")"` (`parser/parser.y:77`) handles **any** function
name generically. `variance(x)` flows through `convert_op` → uppercased → matched against
the `"VARIANCE"`/`"VAR"` entries in `string_to_op`. No lexer/grammar edits, no pregen-source
regeneration required.

### `VarianceFunc` — what it computes
Population variance reduced to a **single scalar per writer block**, numerically-stable
two-pass algorithm in double precision:

```
Var = (1/N) * sum_i (x_i - mean)^2
```

- Output type is floating point (`double`, or `long double` for `long double` input),
  regardless of input type — wired via `FloatTypeFunc`.
- `VarianceDimsFunc` returns `Start{0}, Count{1}, Shape{1}` — i.e. every block reduces its
  local field to one value at global index 0.
- Guards: exactly one operand; `dataSize > 0` guarded to avoid divide-by-zero.

### Build / install state (the one gap that was fixed)
The operator was implemented and installed, but the source had been edited again *after* the
last install, leaving the installed library stale relative to source. Resolved by re-running
`cmake --install build`. Verified functionally identical build vs. install:

- Same **GNU build-id** (`cc5bd8c7abed3fe68ff5e58ea86cf8044dc86997`)
- Same dynamic symbol count (10 656)
- `nm -DC install/lib/libadios2_core.so.2.11 | grep VarianceFunc` → symbol present

(The md5 differed only because CMake rewrites RPATH on install — functionally irrelevant.)

coeus-adapter links this install via
`ADIOS2_DIR=.../ADIOS2/install/lib/cmake/adios2`.

---

## 3. Where it is consumed (gray-scott side)

`simulation/writer.cpp` (in the `Writer` constructor, gated on the `derived` CLI flag)
declares four derived variables:

```cpp
io.DefineDerivedVariable("derive/AddU", "x = U \n add(x)",      adios2::DerivedVarType::StoreData);
io.DefineDerivedVariable("derive/AddV", "x = V \n add(x)",      adios2::DerivedVarType::StoreData);
io.DefineDerivedVariable("derive/VarU", "x = U \n variance(x)", adios2::DerivedVarType::StoreData);
io.DefineDerivedVariable("derive/VarV", "x = V \n variance(x)", adios2::DerivedVarType::StoreData);
```

`derive/VarV` is the trigger signal; `derive/AddV` (block sum) supplies the per-block means
needed to pool a correct global variance (see §6).

### CLI note / robustness bug
`main.cpp` reads `bool derived = atoi(argv[2]);` but only guards `argc < 2`. The `derived`
flag is therefore **mandatory** — running with just the settings file dereferences a missing
`argv[2]` and crashes. Invoke as:

```
mpirun -n <N> adios2-gray-scott <settings>.json 1
```

---

## 4. Reproduction — the `L=64` run

**Build:**
```
cmake --build build --target adios2-gray-scott
```

**Settings** (`L=64` evaluation params, BP5-to-disk instead of the ParaView inline plugin so
the run is self-contained):

```json
{
  "L": 64, "Du": 0.2, "Dv": 0.1, "F": 0.08, "k": 0.03,
  "dt": 1.0, "plotgap": 50, "steps": 5000, "noise": 0.01,
  "output": "gs.bp", "checkpoint": false,
  "adios_config": "adios2.xml", "mesh_type": "image"
}
```
(`adios2.xml` sets the `SimulationOutput` IO engine to **BP5**.)

**Run** (4 ranks, derived enabled):
```
mpirun --oversubscribe -n 4 adios2-gray-scott settings-l64.json 1
```
- Process layout `2x2x1`, local grid `32x32x64`, 100 outputs, ~16 s wall.
- `use derived variables` printed on every rank → derived path active.

**Inspect** — `bpls -l gs.bp`:
```
double   U            100*{64, 64, 64} = 0.139807 / 1.03621
double   V            100*{64, 64, 64} = 0 / 0.803659
double   derive/AddU  100*{64, 64}     = 5.74238 / 32.262
double   derive/AddV  100*{64, 64}     = 0 / 19.2482
double   derive/VarU  100*{1}          = 4.57649e-05 / 0.157123
double   derive/VarV  100*{1}          = 6.36306e-05 / 0.0879357   ← variance trigger signal
int32_t  step         100*scalar       = 50 / 5000
```

---

## 5. Empirical finding — the trigger signal

`derive/VarV` traced across all 100 outputs shows a clean **rise → peak → collapse**,
capturing the perturbation → pattern → steady-state transition:

| output | sim step | VarV | phase |
| ------ | -------- | ---- | ----- |
| 0  | 50   | 0.0030 | perturbation-dominated |
| 5  | 300  | 0.0183 | onset |
| 10 | 550  | 0.0494 | pattern forming |
| **17** | **900**  | **0.0879 (peak)** | reactive spots fully formed |
| 20 | 1050 | 0.0706 | pattern spreading / relaxing |
| 22 | 1200 | 0.0396 | |
| 25 | 1350 | 0.0060 | collapsing |
| 28 | 1500 | 1.43e-04 | near steady state |
| 29–99 | 1550+ | ~6.4e-05 (flat) | uniform steady state |

**Interpretation:** `variance(V)` climbs ~30× from baseline as reactive spots nucleate and
sharpen (high spatial heterogeneity), peaks when the pattern is most heterogeneous, then
falls ~3 orders of magnitude as the field relaxes toward a near-uniform steady state. The
magnitude and timing of the spike are a robust, cheap detector of the regime transition —
which is precisely what Vigil's statistical trigger exploits.

**Resolution dependence:** in this `L=64` run the peak is at **output ~17**; the paper's
`256³` run fires at **output 20**. Same qualitative signature, timing shifts with resolution
— expected.

---

## 6. Important caveat — `variance` is per-writer-block, not global

`bpls -D gs.bp derive/VarV` reveals **4 blocks per step** (one per MPI rank), each declared
at global shape `{1}` / start `0`:

```
step 0:  block 0 = 0.00297248   block 1 = 0.00295732
         block 2 = 0.00295301   block 3 = 0.00295462
```

Because all four blocks map to the same global index 0, reading `VarV` as a *global* array
surfaces only **one** block's value — it is **not** the global variance of `V`.

### Correct global variance (pooled / parallel variance)
To combine per-block statistics into the exact global variance you need, per block `b`, the
triple `(mean_b, var_b, N_b)`:

```
N       = sum_b N_b
mean    = (1/N) * sum_b N_b * mean_b
var     = (1/N) * sum_b N_b * ( var_b + (mean_b - mean)^2 )
```

The pieces are all available from the emitted derived variables:
- `var_b`  = `derive/VarV` block `b`
- `mean_b` = `derive/AddV` block `b` / `N_b`   (Add = block sum)
- `N_b`    = product of the block's `Count` dims (known from the layout)

So `derive/VarV` + `derive/AddV` together are sufficient to reconstruct the **exact** global
variance on the reader/trigger side. **Averaging the per-block variances is incorrect** — it
drops the between-block term `(mean_b - mean)^2` and under-reports the true variance.

### Why the single-block proxy looked fine here
For this symmetric, centered-seed initial condition on a `2x2x1` decomposition, the four
per-block variances are nearly identical (0.00297 vs 0.00295), so any single block is a good
proxy. This is a property of the symmetric IC/decomposition, **not** a general guarantee —
asymmetric decompositions or localized features will make the blocks diverge, and only the
pooled formula stays correct.

---

## 7. Status & next steps

- [x] `variance`/`var` operator implemented, registered, parseable, built, installed
- [x] gray-scott emits `derive/VarV`, `derive/VarU`, `derive/AddV`, `derive/AddU`
- [x] `L=64` end-to-end run verified; trigger signal confirmed (peak ~30× baseline)
- [ ] **Commit** the ADIOS2 variance changes (still uncommitted local edits)
- [ ] Reader/trigger side: implement the **pooled** global-variance combine (§6), not a
      per-block average
- [ ] (optional) Fix `main.cpp` to validate `argc >= 3` before reading the `derived` flag

---

## 8. Reference — key file locations

| What | Path |
| ---- | ---- |
| Variance compute/dims | `../ADIOS2/source/adios2/toolkit/derived/Function.cpp` (`VarianceFunc`, `VarianceDimsFunc`) |
| Operator registration | `../ADIOS2/source/adios2/toolkit/derived/Expression.cpp` (`string_to_op`, `OpFunctions`) |
| Parser grammar | `../ADIOS2/source/adios2/toolkit/derived/parser/parser.y` (rule at line 77) |
| Derived-var declarations | `simulation/writer.cpp` (`Writer` ctor, `derived` branch) |
| CLI / derived flag | `simulation/main.cpp` (`argv[2]`) |
| Installed library | `../ADIOS2/install/lib/libadios2_core.so.2.11` |
| `bpls` tool | `../ADIOS2/install/bin/bpls` |
