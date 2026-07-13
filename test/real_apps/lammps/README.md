# LAMMPS through the hermes engine — temperature (mean) trigger

Wires the LAMMPS velocity-Verlet integration-failure case (Vigil LAMMPS case)
into the COEUS trigger-render-reason pipeline: LAMMPS `dump custom/adios`
writes through the **hermes plugin engine**, which evaluates a **mean trigger**
on the kinetic temperature and gates the SST render stream.

## The derived variable

The modified dump (`src/ADIOS/dump_custom_adios.cpp`, our +adios LAMMPS build)
de-interleaves the atom table into named per-column variables and declares

```
derive/V2mean = mean(multiply(magnitude(vx,vy,vz), magnitude(vx,vy,vz)))
              = <|v|^2>  ==  3 * T*      (LJ reduced units, m=1, k_B=1)
```

whenever `vx vy vz` are dumped. This is the velocity-field analog of
Gray-Scott's `variance(V)`. Verified: ADIOS2's own computed value matches
LAMMPS thermo T* to 4 sig figs, including the 2e6 explosion spike.

## The trigger (engine side)

`TriggerType=mean` (added to `hermes_engine.cc`) pools the derived per-block
mean N_b-weighted into the exact global mean (`ComputeGlobalBlockMean_`, shared
with the dissipation trigger) and fires on the first upward crossing of
`TriggerThreshold` (absolute, in 3*T* units) or `TriggerBaselineRatio` x the
first-step baseline — sharing the rising-edge / inspect-window / SST-gating
path with the variance trigger. Threshold `4.5` => fires once T* >= 1.5.

## Files

| File | Purpose |
|---|---|
| `adios2_config.xml` | Binds IO group `custom` to the hermes plugin + mean-trigger knobs |
| `in.lj_explosion_hermes` | Dilute LJ, dt*=0.05 explosion; dumps `id x y z vx vy vz` |

## Run (writer side)

Prerequisites: rebuild the engine with the `TriggerType=mean` support
(`cmake --build build` in coeus-adapter), and bring up the clio runtime + CTE
pool exactly as in `docs/BUILD_AND_RUN_GRAY_SCOTT.md` §2. The LAMMPS binary is
the +adios build at `~/software/lammps_bench/lammps/build/lmp`.

```bash
spack load iowarp@main adios2-coeus@vigil openmpi
cd test/real_apps/lammps
mpirun -n 1 ~/software/lammps_bench/lammps/build/lmp -in in.lj_explosion_hermes
```

Expected: engine prints `Trigger: mean(derive/V2mean) threshold=4.5 ...` at
init, then `Trigger FIRED at step N: mean(derive/V2mean) = ...` on the
super-heated step, with a JSON-lines fire event in `lammps_trigger_log.jsonl`.
Untriggered steps log nothing.

## Render side (not yet wired)

To ship flagged steps to ParaView, uncomment the Catalyst block in
`adios2_config.xml` (`DataModel` + `Script` + `CatalystStream`). This needs a
**Fides particle data model** for the atoms (the Gray-Scott uniform-grid
`gs-fides.json` does not apply) plus a ParaView pipeline script; `x y z` are
already dumped for that purpose. Multi-rank runs pool correctly (the mean is
N_b-weighted per writer block).

## Caveats

- Temperature fires the **explosion** (dt*=0.05). The subtle dense drift
  (rho*=0.8442, dt*=0.02) injects energy into *potential* energy while T* stays
  ~0.76 — that sub-case needs a per-atom PE derived signal (`compute pe/atom`),
  not temperature.
- `derive/V2mean` is 3*T*; thresholds are in those units (divide by 3 for T*).
