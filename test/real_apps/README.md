[← COEUS-Adapter README](../../README.md) · [Trigger-Render-Reason Pipeline](../../docs/TRIGGER_RENDER_REASON_PIPELINE.md)

# Real-application cases: Trigger-Render-Reason (Vigil)

Each subdirectory here is a **worked example** of the COEUS
[Trigger-Render-Reason pipeline](../../docs/TRIGGER_RENDER_REASON_PIPELINE.md)
(*Vigil*): a running simulation writes through the Coeus `hermes` engine, the
engine evaluates a **global statistic** every output step (**trigger**), streams
only the flagged window over ADIOS2 **SST** to a consumer (**render**), and an
AI agent inspects the streamed steps and issues a verdict: stop, or *rescue*
the run (**reason**).

The four cases differ in the physics, the trigger statistic, and the reason
action; the gate/window/SST machinery in
[`src/hermes_engine.cc`](../../src/hermes_engine.cc) is shared.

## The four cases

| Application | Dir | `TriggerType` | Trigger signal | Fires on | Reason action | End-to-end status |
|---|---|---|---|---|---|---|
| **Gray-Scott** (reaction-diffusion) | [`gray-scott/`](gray-scott/README.md) | `variance` | pooled `variance(V)` (`derive/VarV`) | reaction-pattern collapse to steady state | agent **stops** (`fire_stop`) | ✅ verified, incl. async render-reason (2026-07-20) |
| **LAMMPS** (molecular dynamics) | [`lammps/`](lammps/README.md) | `mean` / `variance` | `mean(\|v\|²) ≈ 3T*` (or `variance(vx)`) | velocity-Verlet blowup ("Lost atoms") | agent **stops** | ✅ trigger verified (2026-07-12); ParaView-free render+reason consumer added (offline BP5-testable) |
| **Xcompact3d** TGV (CFD) | [`Xcompact3d/`](Xcompact3d/README.md) | `dissipation` | two-stage Yellow/Red `eps_frac` / `nu_ratio` | silent **numerical** dissipation on under-resolved TGV | agent **stops** | ✅ gated fire verified 1-rank & 25-rank (2026-07-10); Q-criterion bridge TODO |
| **LBM-CFD 2D** (lattice-Boltzmann) | [`ascent-trame/`](ascent-trame/README.md) | `variance` | `variance(vorticity)` (`derive/VarVort`) | D2Q9 instability onset (grid-scale speckle) | agent **rescues** (revert + 2× steps) *or* stops | ✅ verified end-to-end through the engine (2026-07-16) |

The **LBM-CFD** case is the distinctive one: its reason action can **repair** the
run, not only halt it. The agent calls `fire_rescue_simulation`, which reverts to the last
checkpoint and doubles the timestep count (halving lattice speed to restabilise
the scheme). It is also the worked example of all three
stages in one continuous run.

## Per-case documents

Each case's `README.md` is the authoritative entry point. The other files are
detailed dev logs / references kept alongside.

### Gray-Scott: `variance` (pattern collapse)
- [`gray-scott/README.md`](gray-scott/README.md) - entry point (upstream ADIOS2 docs + Vigil overview)
- [`gray-scott/VARIANCE_TRIGGER.md`](gray-scott/VARIANCE_TRIGGER.md) - the `variance` operator implementation, pooled-global-variance trigger internals, **§8 asynchronous (non-blocking) render-reason** + per-part timing
- [`../../docs/BUILD_AND_RUN_GRAY_SCOTT.md`](../../docs/BUILD_AND_RUN_GRAY_SCOTT.md) - full end-to-end walkthrough (build → run → gated SST → agent → early stop)
- [`../../docs/ARTIFACT_DESCRIPTION_DELTA.md`](../../docs/ARTIFACT_DESCRIPTION_DELTA.md) - **256-rank scale run on NCSA Delta** (2×128-rank producer + agent consumer, `srun`/PMIx); collapse-warn → agent stop, writer halted at step 6500/20000, `variance(V)` bit-identical at 1/4/256 ranks (verified 2026-07-17)
- jarvis package: [`../jarvis/jarvis_coeus/jarvis_coeus/adios2_gray_scott/`](../jarvis/jarvis_coeus/jarvis_coeus/adios2_gray_scott/)

### LAMMPS: `mean` (kinetic temperature)
- [`lammps/README.md`](lammps/README.md) - authoritative entry point (§5 = render + reason consumer)
- [`lammps/consumer/`](lammps/consumer/) - **ParaView-free render+reason**: `lammps_sst_reader.py` (atom-scatter render) + `lammps_insitu_mcp_server.py` + `lammps_agent.py`
- [`lammps/RUNBOOK.md`](lammps/RUNBOOK.md) - dev log: build/run/internals (verified 2026-07-12)
- [`lammps/DERIVED_QUANTITIES.md`](lammps/DERIVED_QUANTITIES.md) - the derived `variance(vx)` path + the define-time-dims fix
- jarvis package: [`../jarvis/jarvis_coeus/jarvis_coeus/lammps/`](../jarvis/jarvis_coeus/jarvis_coeus/lammps/)

### Xcompact3d: `dissipation` (numerical dissipation)
- [`Xcompact3d/README.md`](Xcompact3d/README.md) - authoritative entry point
- [`Xcompact3d/catalyst/TGV_INSITU_AGENT.md`](Xcompact3d/catalyst/TGV_INSITU_AGENT.md) - interactive AI-agent consumer (pvserver + bridge + MCP)
- [`Xcompact3d/Incompact3d/adios2_derived_metrics_guide.md`](Xcompact3d/Incompact3d/adios2_derived_metrics_guide.md) - full derived-offload guide (ops, API, measurements)
- [`Xcompact3d/Incompact3d/derived_comparison.md`](Xcompact3d/Incompact3d/derived_comparison.md) · [`derived_quantities.md`](Xcompact3d/Incompact3d/derived_quantities.md) - native vs ADIOS2 head-to-heads, native formulas
- [`Xcompact3d/Incompact3d/runs/eval2_report.md`](Xcompact3d/Incompact3d/runs/eval2_report.md) - under-resolved TGV evaluation timeline
- jarvis package: [`../jarvis/jarvis_coeus/jarvis_coeus/Incompact3d/`](../jarvis/jarvis_coeus/jarvis_coeus/Incompact3d/)

### LBM-CFD 2D (ascent-trame): `variance` (instability onset / rescue)
- [`ascent-trame/README.md`](ascent-trame/README.md) - Vigil-first entry point (+ upstream Ascent/Trame build below)
- [`ascent-trame/examples/lbm-cfd/README.md`](ascent-trame/examples/lbm-cfd/README.md) - **the authoritative case doc**: SST/BP5, derived `VarVort`, ParaView-free consumer + rescue/stop agent
- [`ascent-trame/examples/lbm-cfd-3d/README.md`](ascent-trame/examples/lbm-cfd-3d/README.md) - 3D LBM variant (ParaView VTS path)
- jarvis package: [`../jarvis/jarvis_coeus/jarvis_coeus/lbm_cfd/`](../jarvis/jarvis_coeus/jarvis_coeus/lbm_cfd/)

## Shared render + reason stack: [`../insitu_agent/`](../insitu_agent/README.md)

The ParaView/MCP AI-agent consumer used by the Gray-Scott case (the *render* and
*reason* side) and the scalability / agent-vs-baseline evaluations live here.
See its [`README.md`](../insitu_agent/README.md) for the unified pipeline
overview; supporting material is organized under
[`assets/`](../insitu_agent/assets/) (`xml/`, `configs/`, `scripts/`, `docs/`):

- [`../insitu_agent/README.md`](../insitu_agent/README.md) - unified pipeline README: architecture, quick start, MCP tools, layout
- [`../insitu_agent/assets/docs/PIPELINE_TIMELINE.md`](../insitu_agent/assets/docs/PIPELINE_TIMELINE.md) - async pipeline timeline analysis (128-rank run, single wall clock, per-part breakdown)
- [`../insitu_agent/assets/docs/SCALABILITY_EVAL_PLAN.md`](../insitu_agent/assets/docs/SCALABILITY_EVAL_PLAN.md) - scalability evaluation plan (sim nodes × reader nodes)
- [`../insitu_agent/assets/docs/AGENT_VS_BASELINE_PLAN.md`](../insitu_agent/assets/docs/AGENT_VS_BASELINE_PLAN.md) · [`RUN_BL_VS_AG_PLAN.md`](../insitu_agent/assets/docs/RUN_BL_VS_AG_PLAN.md) - LLM agent vs fixed-schedule ParaView baseline
- [`../insitu_agent/assets/docs/eval3_ares_report.md`](../insitu_agent/assets/docs/eval3_ares_report.md) - Ares run report (`F=0.08/k=0.03`, 3-output LLM intercept)

## Related top-level docs

- [Trigger-Render-Reason Pipeline](../../docs/TRIGGER_RENDER_REASON_PIPELINE.md) - the pipeline concept, trigger types, and configuration (start here)
- [Derived Variables and Hash](../../docs/DERIVED_VARIABLES_AND_HASH.md) - the derived-variable operators the triggers read
- [Operators](../../docs/OPERATORS.md) · [In-Situ Visualization](../../docs/IN_SITU_VISUALIZATION.md)
- [Supported Applications](../../docs/SUPPORTED_APPLICATIONS.md) - broader jarvis-package matrix (also lists WRF, OpenFOAM)
