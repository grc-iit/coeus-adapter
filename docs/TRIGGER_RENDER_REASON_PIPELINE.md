[← COEUS-Adapter README](../README.md)

# Trigger-Render-Reason Pipeline

*(Also called **Vigil**.)* A closed loop that turns a running simulation into a
self-steering one — **detect an event, stream it out, and decide what to do**:

1. **Trigger** — every step, the engine evaluates a global statistic over a
   variable (collective across ranks). When it crosses a threshold (or a
   baseline ratio, or *collapses* back down), the step is flagged.
2. **Render** — the flagged window (the firing step + a few more) is streamed
   over ADIOS2 **SST** to an external consumer: a ParaView/Catalyst viewer
   and/or an AI agent.
3. **Reason** — the AI agent inspects the streamed steps and issues a verdict —
   e.g. calls the MCP tool `fire_stop_simulation`, which writes a `.stop` flag
   the simulation polls, halting the run early.

## Trigger types

Selected with the `TriggerType` engine parameter:

| `TriggerType` | Statistic | Typical use |
|---|---|---|
| `variance` (default) | Exact pooled global variance of `TriggerVariable` | Gray-Scott pattern collapse |
| `mean` | N_b-weighted global mean of a derived per-block mean | LAMMPS kinetic temperature (`mean(|v|²) ≈ 3T*`) |
| `dissipation` | Two-stage Yellow/Red numerical-dissipation metric | Xcompact3d TGV (numerical vs. physical dissipation) |

A **collapse-warning** mode (`TriggerWarnOnCollapse=true`) *arms* when the
statistic rises past a baseline ratio (structure formed) and *warns* when it
falls back down (the field is homogenising toward blank) — the agent then issues
the fire verdict.

The `variance` and `mean` statistics are read from ADIOS2
[derived variables](DERIVED_VARIABLES_AND_HASH.md) (e.g. `derive/VarV`).

## Example applications

Each trigger type has a worked example under `test/real_apps/` — build, run, and
configuration for that app:

| Trigger | Application | Guide |
|---|---|---|
| `variance` | Gray-Scott (pattern collapse) | [gray-scott](../test/real_apps/gray-scott/README.md) · [full walkthrough](BUILD_AND_RUN_GRAY_SCOTT.md) |
| `dissipation` | Xcompact3d TGV (numerical dissipation) | [Xcompact3d](../test/real_apps/Xcompact3d/README.md) |
| `mean` | LAMMPS (kinetic temperature) | [lammps](../test/real_apps/lammps/README.md) |
| `variance` | LBM-CFD 2D (instability onset) | [lbm-cfd](../test/real_apps/ascent-trame/examples/lbm-cfd/README.md) |

The **LBM-CFD 2D** case demonstrates a distinct *reason* action — the agent can
**repair** the run instead of only stopping it. Its `variance(vorticity)` trigger
streams the flagged chaos-onset window to the agent; the agent looks at the
rendered frame and calls `fire_rescue_simulation`, which writes a `.rescue` flag
the simulation polls to **revert to its last checkpoint and double the timesteps**
(halving the lattice speed to restabilise the D2Q9 scheme). `fire_stop_simulation`
remains available for the halt verdict.

It is also the worked example of all three stages in **one continuous run**
(`jarvis ppl load yaml test/jarvis/jarvis_coeus/pipelines/lbm-cfd-agent.yaml`):

```
TRIGGER  engine: variance(derive/VarVort) = 209.2 >= 1.0  -> fires at output 2
RENDER   engine ships exactly the 3 flagged steps over gated SST (nothing before
         the fire), each carrying vigil/trigger_fired / _stat / _fire_step
REASON   agent sees the frame ("grid-scale salt-and-pepper speckle ... no
         coherent von Karman street") -> fire_rescue_simulation
         -> sim reverts to checkpoint 0, 6000->12000 steps -> run recovers
```

The agent side (`consumer/lbm_agent.py` + `lbm_insitu_mcp_server.py`) is
ParaView-free: the reader publishes a rendered PNG + stats, and the MCP server
hands the image to the model via `get_frame_image`, so the verdict is made from
the picture, not just the numbers.

## Configuration

Triggers are configured as engine parameters in the ADIOS2 XML. A minimal
variance trigger:

```xml
<engine type="Plugin">
    <parameter key="PluginName" value="hermes"/>
    <parameter key="PluginLibrary" value="hermes_engine"/>

    <parameter key="TriggerType"          value="variance"/>
    <parameter key="TriggerVariable"      value="derive/VarV"/>  <!-- variance(V) -->
    <parameter key="TriggerThreshold"     value="0"/>            <!-- 0 disables absolute test -->
    <parameter key="TriggerBaselineRatio" value="20"/>           <!-- fire at 20x first-step baseline -->
    <parameter key="TriggerInspectSteps"  value="4"/>            <!-- stream 4 steps on fire -->
    <parameter key="TriggerRefire"        value="false"/>
    <parameter key="TriggerLogFile"       value="trigger_log.jsonl"/>
</engine>
```

Key knobs: `TriggerVariable`, `TriggerThreshold`, `TriggerBaselineRatio`,
`TriggerInspectSteps`, `TriggerRefire`, `TriggerWarnOnCollapse` +
`TriggerCollapseThreshold` / `TriggerCollapseBaselineRatio`, and the dissipation
set (`TriggerKEVariable`, `TriggerEnstrophyVariable`, `TriggerNu`,
`TriggerOutputDt`, `TriggerYellow/RedFraction`, `TriggerYellow/RedNuRatio`).

## Learn more / try it

- **[BUILD_AND_RUN_GRAY_SCOTT.md](BUILD_AND_RUN_GRAY_SCOTT.md)** — full
  walkthrough, including the collapse-warning → agent → early-stop demo (§7).
- **[test/real_apps/gray-scott/VARIANCE_TRIGGER.md](../test/real_apps/gray-scott/VARIANCE_TRIGGER.md)** — trigger config reference.
- **[Ready-to-run pipeline](../test/jarvis/jarvis_coeus/pipelines/gray-scott-warn-collapse.yaml)** —
  `jarvis ppl load yaml <file> && jarvis ppl run`.
- **[test/insitu_agent](../test/insitu_agent)** — the AI agent (render + reason stages).
