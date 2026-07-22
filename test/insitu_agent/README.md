[← real_apps cases](../real_apps/README.md) · [Trigger-Render-Reason Pipeline](../../docs/TRIGGER_RENDER_REASON_PIPELINE.md) · [Gray-Scott case](../real_apps/gray-scott/README.md)

# In-Situ AI-Agent Consumer: Render + Reason

This directory is the **render + reason** consumer stack of the COEUS
[Trigger-Render-Reason pipeline](../../docs/TRIGGER_RENDER_REASON_PIPELINE.md)
(*Vigil*). A Gray-Scott simulation streams its flagged window over ADIOS2 **SST**;
here a ParaView streaming bridge renders those steps and an **AI agent** (via an
MCP server) inspects them and issues a verdict, e.g. `fire_stop_simulation`,
which early-stops the run.

The three moving parts:

- **`insitu_streaming.py`** - streaming bridge: reads the SST stream through a
  Fides data model into a headless **pvserver**, renders each step, and (async
  mode) writes frames to disk.
- **`insitu_mcp_server.py`** - MCP server: exposes streaming control + ParaView
  visualization + frame-inspection tools to any MCP client.
- **`insitu_agent.py`** - the LLM driver: connects to the MCP server, sees the
  streamed frames, and calls the verdict tool.

These three **entry-point scripts stay at the top level**; everything else
(engine/sim configs, batch run-scripts, evaluation reports) is organized under
[`assets/`](assets/).

## Repository layout

```
test/insitu_agent/
├── README.md                 # this file - unified pipeline reference
├── insitu_streaming.py       # SST → pvserver streaming bridge (core)
├── insitu_mcp_server.py      # MCP server: streaming + ParaView + frame tools (core)
├── insitu_agent.py           # LLM agent driver (core)
├── requirements.txt          # python deps for the agent + MCP server
├── prompts/                  # agent prompt texts (agent_*.txt)
├── results/  agent_results/  # run outputs (timings, token usage, frames)
├── streaming_status.json     # runtime state (auto-generated)
└── assets/
    ├── xml/                  # ADIOS2 engine configs: adios2-sst*.xml
    ├── configs/              # sim + Fides configs: settings-*.json, gs-fides.json
    ├── scripts/              # batch run-scripts (run_*.sh) + helpers
    │                         #   (analyze_timeline.py, run_baseline.py, probe_mcp_view.py)
    └── docs/                 # historical records: timeline, eval plans, run reports
```

> The batch run-scripts in `assets/scripts/` are written to be launched **from
> this `insitu_agent/` directory** (they resolve `SCRIPT_DIR` two levels up and
> reference configs via `assets/configs/`, the streamed `gs.bp` and
> `streaming_status.json` at the top level). Run them as
> `bash assets/scripts/run_<name>.sh` from here.

## Architecture

```
┌──────────────────────┐       ADIOS2 SST        ┌─────────────────────────┐
│  Gray-Scott Sim      │ ──── "gs.bp" ─────────►  │  insitu_streaming.py    │
│  (adios2-gray-scott) │                           │  (pvpython on pvserver) │
│                      │  gs.bp.sst contact file   │                         │
│  BeginStep/EndStep   │                           │  Fides reader → pvserver│
└──────────────────────┘                           └────────────┬────────────┘
                                                                │
                                                         pvserver (port 11111)
                                                         --multi-clients
                                                                │
                                                   ┌────────────┴────────────┐
                                                   │                         │
                                            ┌──────┴──────┐          ┌──────┴──────┐
                                            │ ParaView GUI│          │ insitu_mcp  │
                                            │ (optional)  │          │ _server.py  │
                                            └─────────────┘          └──────┬──────┘
                                                                            │
                                                                      MCP Protocol
                                                                            │
                                                                     ┌──────┴──────┐
                                                                     │  AI Agent   │
                                                                     │ (insitu_    │
                                                                     │  agent.py)  │
                                                                     └─────────────┘
```

## Prerequisites

- ParaView with `pvserver` and `pvpython` (conda `conda-forge::paraview`, or a
  headless OSMesa build for GPU-less nodes)
- ADIOS2 with SST support
- Gray-Scott simulation binary (`adios2-gray-scott`)
- Python packages: `mcp[cli]`, `httpx`, `anthropic`/`openai` - `pip install -r requirements.txt`

## Quick start

Run every command **from this `insitu_agent/` directory**.

### 1. Start pvserver

```bash
pvserver --multi-clients --server-port=11111
```

### 2. (Optional) connect the ParaView GUI

Open ParaView GUI → File → Connect → `localhost:11111` to watch the visualization
live alongside the agent.

### 3. Start the Gray-Scott simulation (writer)

```bash
mpirun -n 4 adios2-gray-scott assets/configs/settings-staging.json
```

Writes to the SST stream `gs.bp` using `assets/xml/adios2-sst.xml` (referenced by
`adios_config` inside the settings file).

### 4. Start the streaming bridge

```bash
pvpython insitu_streaming.py \
    -j assets/configs/gs-fides.json \
    -b gs.bp \
    --staging \
    --server localhost \
    --port 11111 \
    --paused
```

`--paused` starts paused so the agent controls stepping. Remove it for
auto-advance (2 s/step). For **async (non-blocking)** mode, drop `--paused` and
add `--frames-dir <dir>` (see below).

### 5. Run the AI agent

The agent launches the MCP server internally.

```bash
# Anthropic
export ANTHROPIC_API_KEY=sk-ant-...
python insitu_agent.py --provider anthropic --model claude-haiku-4-5-20251001

# OpenAI
export OPENAI_API_KEY=sk-...
python insitu_agent.py --provider openai --model gpt-4o

# Single-shot
python insitu_agent.py --provider anthropic --prompt "Create an isosurface of V at 0.5 and take a screenshot"
```

### Alternative: MCP server with Cursor / Claude Desktop

Instead of `insitu_agent.py`, point any MCP client at the server:

```json
{
  "mcpServers": {
    "InSitu-ParaView": {
      "command": "python",
      "args": [
        "/path/to/test/insitu_agent/insitu_mcp_server.py",
        "--server", "localhost",
        "--port", "11111",
        "--status-file", "/path/to/test/insitu_agent/streaming_status.json"
      ]
    }
  }
}
```

## Synchronous vs asynchronous render-reason

| | Synchronous (`--paused` + `advance_step`) | Asynchronous (`--frames-dir`) |
|---|---|---|
| What paces the sim | the agent's `advance_step` calls | nothing - bridge drains at its own speed |
| Agent on sim critical path? | yes | **no** - reads frames from disk |
| Sim stall during flagged window | LLM-paced (~65 s at 128 ranks) | bridge-drain only (~7–17 s) |

In **async** mode the bridge (`insitu_streaming.py --frames-dir <dir>`) drains the
flagged window as fast as SST + render allow, writing `frame_<NNNN>.png` + a
`frames.jsonl` manifest; the agent (`insitu_agent.py --frames-dir <dir>`) issues
its verdict from the on-disk frames via the blocking `get_flagged_frames` tool,
fully off the simulation's critical path. Full mechanism + per-part timing:
[`assets/docs/PIPELINE_TIMELINE.md`](assets/docs/PIPELINE_TIMELINE.md) and
[gray-scott `VARIANCE_TRIGGER.md` §8](../real_apps/gray-scott/VARIANCE_TRIGGER.md).

## MCP tools

### Streaming control
| Tool | Description |
|------|-------------|
| `get_streaming_status` | current timestep, pause state, stream status |
| `pause_streaming` | pause the stream to explore current data |
| `resume_streaming` | resume auto-advancing |
| `advance_step` | advance exactly one timestep, then pause |

### Visualization
| Tool | Description |
|------|-------------|
| `create_isosurface` | isosurface on live data (field, value) |
| `create_slice` | slice through the volume at any plane |
| `toggle_volume_rendering` | enable/disable volume rendering |
| `color_by` · `set_color_map` · `edit_volume_opacity` | color / transfer functions |
| `create_streamline` | streamline visualization |

### Inspection / verdict
| Tool | Description |
|------|-------------|
| `get_screenshot` | capture current view as an image |
| `get_pipeline` · `get_available_arrays` · `compute_surface_area` | pipeline / arrays / mesh area |
| `get_flagged_frames` · `get_flagged_frames_info` | (async) captured flagged-window frames + V ranges |
| `fire_stop_simulation` | write the `.stop` flag that early-stops the run |

## Batch runs & evaluations

Reproducible experiment drivers live in [`assets/scripts/`](assets/scripts/) and
write to `results/`:

| Script | What it runs |
|---|---|
| `run_scaling_test.sh` · `run_scaling_test_B.sh` | infrastructure scaling (producer × consumer ranks) |
| `run_L256_pg20_haiku_8shots*.sh` · `run_pg20_haiku_8shots*.sh` | L=256 / L=512, Haiku 8-shot agent (Block vs Discard) |
| `run_bl_vs_ag_512.sh` · `run_all_512.sh` · `run_sonnet_only.sh` | agent vs fixed-schedule baseline |
| `run_gs_F008k003_inspect_20_22.sh` | the `F=0.08/k=0.03` collapse-window intercept |
| `run_baseline.py` | baseline (fixed-schedule) visualization driver |
| `analyze_timeline.py <results_dir>` | build the single-wall-clock timeline (→ PIPELINE_TIMELINE.md) |
| `probe_mcp_view.py` | LLM-free A/B regression harness for the MCP view |

Design notes and run reports (historical records) are in
[`assets/docs/`](assets/docs/):

- [`PIPELINE_TIMELINE.md`](assets/docs/PIPELINE_TIMELINE.md) - async pipeline timeline (128-rank, per-part breakdown)
- [`SCALABILITY_EVAL_PLAN.md`](assets/docs/SCALABILITY_EVAL_PLAN.md) - scalability evaluation plan
- [`AGENT_VS_BASELINE_PLAN.md`](assets/docs/AGENT_VS_BASELINE_PLAN.md) · [`RUN_BL_VS_AG_PLAN.md`](assets/docs/RUN_BL_VS_AG_PLAN.md) - agent vs baseline
- [`eval3_ares_report.md`](assets/docs/eval3_ares_report.md) - Ares run report (`F=0.08/k=0.03`)

## SST tuning

Edit the engine config in [`assets/xml/`](assets/xml/) to change coupling behavior
(`adios2-sst.xml` = default; `-blocking` / `-discard` / `-discard-noreader`
variants):

- **`QueueLimit`** - steps buffered (higher = more lag tolerance for the agent).
- **`QueueFullPolicy`** - `Discard` (drop old data, non-blocking sim) or `Block` (sim waits).
- **`RendezvousReaderCount`** - `1` = sim waits for the reader; `0` = sim starts immediately.
- **`DataTransport`** - `WAN` for TCP (cross-node), `MPI` for same-node.

## Related documentation

- [Trigger-Render-Reason Pipeline](../../docs/TRIGGER_RENDER_REASON_PIPELINE.md) - the pipeline concept and trigger types
- [Gray-Scott case](../real_apps/gray-scott/README.md) · [`VARIANCE_TRIGGER.md`](../real_apps/gray-scott/VARIANCE_TRIGGER.md) - the `variance(V)` trigger this consumer pairs with
- [Build & Run Gray-Scott (single node)](../../docs/BUILD_AND_RUN_GRAY_SCOTT.md) - full end-to-end walkthrough
- [Artifact Description - Delta at scale](../../docs/ARTIFACT_DESCRIPTION_DELTA.md) - 256-rank producer + this agent consumer on NCSA Delta
- [real_apps case index](../real_apps/README.md) - all four Vigil cases
