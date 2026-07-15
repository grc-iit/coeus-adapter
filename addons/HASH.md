# hash() — an ADIOS2 derived variable, not a coeus-adapter module

**coeus-adapter needs no code and no libraries for hashing.** `hash()` is a
derived-variable *operation* provided by the state-diff-enabled ADIOS2 fork
(`lizdulac/ADIOS2` branch `coeus-hash`, built with `ADIOS2_USE_Kokkos=ON`,
`StateDiff_ROOT=...`, `ADIOS2_USE_Derived_Variable=ON`). Kokkos and state-diff
are **ADIOS2 build-time dependencies only** — they never appear in
coeus-adapter's link line.

## Why nothing is built into the engine

The engine already computes *any* derived variable in `EndStep()`:

    HermesEngine::EndStep()
      -> ComputeDerivedVariables()                 // hermes_engine.cc:722
           for each io.GetDerivedVariables():
             blobs = tag->Get(source_vars)         // read this step's inputs
             out   = derivedVar->ApplyExpression()  // <-- hash() runs INSIDE adios2
             PutDerived(out)                        // store into CTE + metadata

`hash(x)` flows through this exact path, the same as `variance(x)`, `add(x)`,
`curl(x)`, `magnitude(x)`. The hash tree is produced by ADIOS2 (using state-diff)
and handed back as a normal derived blob. The engine is agnostic to which
operation it was.

## How to enable it (producer side)

Define the derived variable where the other derived vars are declared — e.g.
`test/real_apps/gray-scott/simulation/writer.cpp` (next to `derive/VarV`), or the
standalone analysis tool `test/real_apps/gray-scott/analysis/hashing.cpp` that
already does exactly this:

    io.DefineDerivedVariable("derive/hashV",
                             "x = V \n hash(x)",
                             adios2::DerivedVarType::StoreData);

Run with the coeus-adapter (`hermes`) engine selected, against the state-diff
ADIOS2 fork. `derive/hashV` lands in CTE alongside `V` each step. No engine
change, no `hermes_engine.cc` edit, no new link dependency.

## What the trunk already carries (and what it does not)

The hash() *mechanism* rides the engine's existing derived-variable path, so
nothing new is built into coeus-adapter. The trunk carries the pieces that use
it — but note the binary comes from the ADIOS2 fork, not this build:

* `test/real_apps/gray-scott/analysis/hashing.cpp` (and `wrf_hashing.cpp`) —
  example reader/writer that defines `derive/hashU` / `derive/hashV` and writes
  them out. **These sources are not compiled by the trunk's CMake**
  (`test/real_apps/gray-scott/CMakeLists.txt` builds only `adios2-gray-scott`
  and `inCompact3D_analysis`); they document the pattern.
* `test/jarvis/jarvis_coeus/jarvis_coeus/adios_hashing/` — Jarvis package whose
  `pkg.py` execs the `adios2-hashing` binary and can select `engine=hermes`
  (coeus-adapter) or `bp5`, with `config/{adios2.xml, hermes.xml, operator.yaml,
  var.yaml}`. The `adios2-hashing` binary itself is provided by the
  state-diff-enabled ADIOS2 fork (its hashing example), **not** by
  coeus-adapter's own build.

To hash a gray-scott run through coeus-adapter, run that package with
`engine=hermes` (the `adios2-hashing` binary must be on `PATH` from the ADIOS2
fork) — no coeus-adapter code, no new link dependency, `hermes_engine.cc`
untouched.

## Relationship to RECUP

RECUP's evaluation (`test/recup_evaluation/case1.sh`) used exactly this
`adios_hashing` package + `hash()` derived variable. RECUP *also* contained a
`Put_hash` ChiMod in `coeus_mdm.cc` that linked state-diff/Kokkos directly into
coeus-adapter, but that was an incomplete prototype (it does not compile) and is
**not** the path that was used. This add-on set deliberately does not resurrect
it.
