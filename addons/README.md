# Coeus-adapter add-on operators

The two RECUP / time_derivatives features reach coeus-adapter by **different,
already-existing seams** — neither touches `src/hermes_engine.cc`:

| Feature          | From branch      | Mechanism                                                            |
|------------------|------------------|---------------------------------------------------------------------|
| `hash()`         | RECUP            | ADIOS2 **derived variable** — rides the engine's existing derived path; no code, no state-diff/Kokkos in coeus-adapter. See `HASH.md`. |
| `coeus_tderiv`   | time_derivatives | Standalone **CTE consumer** — reads step-tagged blobs out-of-band.   |

Why the split: an ADIOS2 derived variable is computed within a single step (no
cross-step history), so it can host `hash(x)` but **cannot** express a time
derivative (which needs a 5-step history). The time derivative therefore runs as
an out-of-band consumer; the hash needs nothing but a derived-variable definition.

## How the time-derivative stays out of the engine

The HermesEngine writes every variable of step *N* (rank *r*) into a CTE tag
named `step_<N>_rank<r>`. `coeus_tderiv` attaches to the **same pre-deployed CTE
core pool** as an ordinary CTE client (reusing the engine's unmodified
`CTEHermes` / `CTETagClient`) and reads those blobs back. No engine hook, no
engine edit, no engine relink.

* **Offline** (default): run after a simulation over the persisted step tags.
* **Online** (`--online`): run concurrently; polls CTE (`WaitForStep`) until a
  step's blob is present, so it trails the app near-line.

## Build

```bash
# default build: add-on absent, engine unaffected
cmake ..

# build the time-derivative tool (no extra deps)
cmake .. -DCOEUS_ENABLE_OPERATORS=ON
```

(hash() needs no build flag here — enable it by defining the derived variable
in the producer, and build against the state-diff ADIOS2 fork. See `HASH.md`.)

## Run

```bash
# after (or alongside, with --online) an Xcompact3d/Incompact3d run that writes
# the pressure field `pp` over 4 ranks / 100 output steps (dt = ioutput*dt_sim):
coeus_tderiv --var pp --ranks 4 --steps 100 --dt 0.05
```

`--var` is generic — point it at any variable the engine wrote (e.g. `--var V`
for a gray-scott run). `pp` is the default because the time_derivatives branch
targeted the Xcompact3d/Incompact3d pressure field.

## Tradeoff (read before choosing this over an in-engine hook)

These run as **consumers**, so they see each step slightly after the engine
writes it (near-line), not strictly inline in the Put call. That is the price
of the hard "don't touch `hermes_engine.cc`" constraint — there is no in-process
operator hook in the current engine. If a *strictly inline* result is required,
the minimal alternative is a ~3-line guarded dispatch in `EndStep()` (invoke the
operator on the step's blobs before the tag is torn down at
`hermes_engine.cc:911`) — at the cost of editing `hermes_engine.cc`.
