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
