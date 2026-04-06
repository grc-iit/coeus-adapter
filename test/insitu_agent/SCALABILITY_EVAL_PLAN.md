# Scalability Evaluation Plan: In-Situ AI Agent Pipeline for Gray-Scott

## Context

We need to evaluate the scalability of the full in-situ AI agent pipeline:

**Gray-Scott simulation (4-8 nodes) -> ADIOS2 SST -> ParaView Fides reader -> MPI-parallel pvserver (1-4 nodes) -> MCP server -> Claude Code (AI agent)**

- Grid size: L=256 (16.8M cells, ~268 MB/step for U+V float64 fields)
- Cluster: Ares (ares-comp-XX nodes, OpenMPI/TCP over eno1)
- Gray-Scott binary: `~/software/gray-scott/build/adios2-gray-scott`
- ParaViewManager: `~/software/paraview_mcp/paraview_manager.py`
- ParaView: loaded via `spack load paraview`
- MCP Python packages: installed alongside spack Python
- AI Agent: **Claude Code** directly via MCP (no SDK, no `insitu_agent.py`)

### Ares MPI Environment

The Ares cluster MPI environment is volatile. All MPI processes (simulation, pvserver, streaming bridge) **must** set these environment variables:

```bash
export OMPI_MCA_pml=ob1
export OMPI_MCA_btl=tcp,self
export OMPI_MCA_osc='^ucx'
export OMPI_MCA_btl_tcp_if_include=eno1
export OMPI_MCA_oob_tcp_if_include=eno1
```

For cross-node `mpirun`, use the SSH wrapper at `~/software/gray-scott/ssh-spack-wrapper.sh` which injects the pre-resolved spack PATH and LD_LIBRARY_PATH:

```bash
mpirun --mca plm_rsh_agent ~/software/gray-scott/ssh-spack-wrapper.sh \
       --mca plm_ssh_no_tree_spawn 1 \
       --host <nodes> -np <N> <command>
```

### Claude Code as the AI Agent

Instead of using the Anthropic Python SDK through `insitu_agent.py`, Claude Code connects directly to `insitu_mcp_server.py` as a native MCP client. This eliminates the SDK layer entirely:

```
Before (SDK):  User -> insitu_agent.py -> Anthropic SDK -> LLM -> MCP client -> MCP server -> pvserver
After (Claude Code):  User -> Claude Code -> MCP server -> pvserver
```

#### MCP Server Configuration (`~/.claude/settings.json`)

```json
{
  "mcpServers": {
    "InSitu-ParaView": {
      "command": "/mnt/common/hxu40/spack/.../bin/pvpython",
      "args": [
        "/mnt/common/hxu40/coeus/iowarp/coeus-adapter/test/insitu_agent/insitu_mcp_server.py",
        "--port", "11112",
        "--status-file", "/mnt/common/hxu40/coeus/iowarp/coeus-adapter/test/insitu_agent/streaming_status.json"
      ]
    }
  }
}
```

This gives Claude Code direct access to all 24 MCP tools: streaming control (pause, resume, advance_step, get_streaming_status) + ParaView visualization (isosurface, slice, screenshot, etc.).

---

## 1. Test Matrix

### Dimension A: Simulation Scale (reader fixed at 1 node)

| ID | Sim Nodes | nprocs | ppn | L   | steps | Output Steps | Purpose                     |
|----|-----------|--------|-----|-----|-------|--------------|-----------------------------|
| A1 | 2         | 32     | 16  | 256 | 200   | 20           | Quarter-scale baseline      |
| A2 | 4         | 64     | 16  | 256 | 200   | 20           | Half-scale                  |
| A3 | 8         | 128    | 16  | 256 | 200   | 20           | Target scale                |

### Dimension B: Reader/pvserver Scaling (sim fixed at A3: 8 nodes, 128 procs, L=256)

| ID | pvserver Nodes | pvserver nprocs | Screenshots | Purpose                     |
|----|---------------|----------------|-------------|-----------------------------|
| B1 | 1             | 1              | Yes         | Single-process baseline     |
| B2 | 1             | 16             | Yes         | Intra-node parallel pvserver|
| B3 | 2             | 32             | Try*        | 2-node parallel pvserver    |

> **Start with B1/B2** (1 node — safe), then attempt B3 (2 nodes). Multi-node pvserver
> requires **IceT parallel image compositing** for screenshots — see note below.
>
> *B3 screenshots: attempt with `--force-offscreen-rendering`. If IceT hangs, fall back
> to `scripted_headless` (no screenshots). We can also try tuning `ICET_STRATEGY` or
> `PV_ICET_COMPOSITING_STRATEGY` env vars.

#### What is IceT and why it matters

IceT (Image Composition Engine for Tiles) is ParaView's library for compositing
partial images from distributed MPI ranks into a single final image. When pvserver
runs N ranks across multiple nodes:

1. Each rank renders its local data partition into a partial (RGBA) image
2. IceT uses MPI collectives (binary-tree allreduce) to exchange and blend image tiles
3. Rank 0 produces the final composited image (used by `SaveScreenshot`)

On **1 node**, IceT compositing uses shared-memory MPI — fast and reliable.
On **2+ nodes**, IceT needs cross-node MPI collectives over TCP (`eno1`).
This is where hangs occur on Ares: the OpenMPI/TCP transport can deadlock
during IceT's image exchange, especially with large framebuffers.

---

## 2. Architecture Per Run

```
[Sim nodes: ares-comp-25..29,17..20]     [Reader node: ares-comp-22 (+23 for B3)]
  mpirun -n 64-128                         pvserver --multi-clients
  adios2-gray-scott                               |
        |                                  [ares-comp-21: Claude Code host]
        |--- SST (WAN/TCP) -------->       pvpython insitu_streaming.py
        |    gs.bp / gs.bp.sst                    |  (connects to pvserver, reads Fides)
                                           insitu_mcp_server.py (via MCP stdio)
                                                  |
                                           Claude Code  <-- user interaction
                                                  (calls MCP tools directly,
                                                   analyzes screenshots via vision)
```

### Node Assignments (actual available nodes)

| Role | Nodes | Notes |
|------|-------|-------|
| **Claude Code + MCP server + streaming bridge** | ares-comp-21 | Our session host |
| **pvserver** | ares-comp-22 (+ ares-comp-23 for B3) | Reader/renderer |
| **Simulation (A1: 2 nodes)** | ares-comp-25, 26 | 32 procs |
| **Simulation (A2: 4 nodes)** | ares-comp-25..28 | 64 procs |
| **Simulation (A3: 8 nodes)** | ares-comp-25..29, 17..19 | 128 procs |

### Claude Code as Image Analyzer

Claude Code has native multimodal vision — it can read and analyze screenshots
directly using the `Read` tool on image files. This eliminates the need for an
external API or SDK for image analysis:

```
MCP get_screenshot → saves PNG to disk → Claude Code Read tool → visual analysis
```

No `eval_harness.py` or Anthropic SDK needed for interactive evaluation sessions.
Claude Code calls MCP tools directly and describes what it sees in each screenshot.

### Concrete Paths

| Item | Path |
|------|------|
| Gray-Scott binary | `~/software/gray-scott/build/adios2-gray-scott` |
| Gray-Scott settings (reference) | `~/software/gray-scott/settings-sst.json` (L=256) |
| ADIOS2 SST XML (reference) | `~/software/gray-scott/adios2-fides-staging.xml` |
| Fides JSON (reference) | `~/software/gray-scott/catalyst/gs-fides.json` |
| SSH wrapper | `~/software/gray-scott/ssh-spack-wrapper.sh` |
| ParaViewManager | `~/software/paraview_mcp/paraview_manager.py` |
| Existing run script (reference) | `~/software/gray-scott/run-sst.sh` |
| Streaming bridge | `test/insitu_agent/insitu_streaming.py` |
| MCP server | `test/insitu_agent/insitu_mcp_server.py` |

### Launch Sequence (strict order)

1. **Load environment**:
   ```bash
   eval $(spack load --sh adios2@2.11.0/g)
   spack load paraview
   export OMPI_MCA_pml=ob1
   export OMPI_MCA_btl=tcp,self
   export OMPI_MCA_osc='^ucx'
   export OMPI_MCA_btl_tcp_if_include=eno1
   export OMPI_MCA_oob_tcp_if_include=eno1
   ```

2. **pvserver** on reader node (ares-comp-22):
   ```bash
   # B1: single process
   ssh ares-comp-22 "eval \$(spack load --sh paraview); \
       pvserver --multi-clients --server-port=11112 --force-offscreen-rendering"

   # B2: 16 processes (intra-node)
   mpirun --mca plm_rsh_agent ~/software/gray-scott/ssh-spack-wrapper.sh \
       --mca plm_ssh_no_tree_spawn 1 \
       --host ares-comp-22:16 -np 16 \
       pvserver --multi-clients --server-port=11112 --force-offscreen-rendering
   ```

3. **Gray-Scott simulation** on sim nodes:
   ```bash
   # A2: 4 nodes, 64 procs
   cd ~/software/gray-scott
   mpirun --mca plm_rsh_agent ~/software/gray-scott/ssh-spack-wrapper.sh \
       --mca plm_ssh_no_tree_spawn 1 \
       --host ares-comp-25:16,ares-comp-26:16,ares-comp-27:16,ares-comp-28:16 \
       -np 64 \
       bash -c "
           export OMPI_MCA_pml=ob1
           export OMPI_MCA_btl=tcp,self
           export OMPI_MCA_osc='^ucx'
           export OMPI_MCA_btl_tcp_if_include=eno1
           export OMPI_MCA_oob_tcp_if_include=eno1
           cd ~/software/gray-scott
           ./build/adios2-gray-scott settings-sst.json 0
       "
   ```

4. **Wait for SST contact file** (`gs.bp.sst`) to appear (poll, timeout 180s)

5. **Streaming bridge** on ares-comp-21 (Claude Code host):
   ```bash
   eval $(spack load --sh paraview)
   pvpython insitu_streaming.py \
       -j gs-fides.json \
       -b ~/software/gray-scott/gs.bp \
       --staging --server ares-comp-22 --port 11112 \
       --paused --enable-timing --timing-file streaming_timing.jsonl
   ```

6. **Claude Code** (already running with MCP server configured):
   - Claude Code calls MCP tools directly: get_streaming_status, advance_step,
     create_isosurface, get_screenshot, etc.
   - Claude Code **analyzes screenshots via built-in vision** (Read tool on PNG files)
     — no external API or `insitu_agent.py` needed
   - For automated benchmarks, use `eval_harness.py`:
     ```bash
     python eval_harness.py --scenario scripted_basic \
         --server-host ares-comp-22 --server-port 11112 \
         --timing-file tool_timing.jsonl
     ```

---

## 3. Metrics to Capture

### 3.1 Two-Level Timing: ParaView vs MCP Overhead

Each MCP tool call has two components. We instrument **both** to isolate costs:

```
|<-------------- MCP total (insitu_mcp_server.py) ------------->|
|  MCP decode  |<-- ParaView operation (paraview_manager) -->|  MCP encode  |
|  + dispatch  |   connect / filter / render / screenshot    |  + return    |
```

- **ParaView operation time**: Instrumented inside `ParaViewManager` methods (the actual `paraview.simple` API calls)
- **MCP total time**: Instrumented as a decorator on each `@mcp.tool()` function (includes ParaView time + MCP serialization/dispatch)
- **MCP overhead** = MCP total - ParaView operation

### 3.2 Full Metric Breakdown

| Layer | Metric | Where to Instrument | What It Measures |
|-------|--------|-------------------|------------------|
| **Simulation** | sim_write_ms/step | Simulation logs | ADIOS2 Put + EndStep (SST send) |
| **SST Transfer** | sst_wait_ms/step | `insitu_streaming.py` around `PrepareNextStep` loop | Time waiting for next SST step to arrive |
| **Fides/Pipeline** | pipeline_update_ms/step | `insitu_streaming.py` around `UpdatePipelineInformation` | Data deserialization + VTK grid construction |
| **Rendering** | render_ms/step | `insitu_streaming.py` around `Render(view)` | ParaView render pass (auto-triggered per step) |
| **ParaView Ops** | pv_op_ms/tool_call | `paraview_manager.py` (each method) | Actual ParaView work: create filter, take screenshot, camera ops, etc. |
| **MCP Total** | mcp_total_ms/tool_call | `insitu_mcp_server.py` (decorator on `@mcp.tool`) | Full tool call including MCP protocol overhead |
| **MCP Overhead** | mcp_overhead_ms/tool_call | Computed: mcp_total - pv_op | MCP serialization, stdio transport, dispatch |
| **System** | memory_mb | `vmstat` on reader nodes | Memory pressure during L=256 ingestion |
| **End-to-End** | total_wall_s | Shell-level timing | Full pipeline duration |

### 3.3 Per-Tool Timing Detail

Each tool call logs a record with both layers:

```json
{
    "tool": "create_isosurface",
    "args": {"value": 0.3, "field": "V"},
    "pv_operation_ms": 142.5,
    "mcp_total_ms": 148.2,
    "mcp_overhead_ms": 5.7,
    "timestamp": 1712345678.123
}
```

This lets us answer:
- **How much does visualization cost?** -> `pv_operation_ms` per tool
- **How much does MCP add?** -> `mcp_overhead_ms` per tool
- **Which operations are expensive?** -> compare `pv_operation_ms` across tools (screenshot vs isosurface vs slice etc.)

### 3.4 Output Files

- `streaming_timing.jsonl` -- per-step: sst_wait_ms, pipeline_update_ms, render_ms
- `mcp_tool_timing.jsonl` -- per-tool-call: pv_operation_ms, mcp_total_ms, mcp_overhead_ms
- `results/scalability_eval.csv` -- consolidated averages per config

---

## 4. Agent Evaluation Scenarios

### `scripted_basic` (5 single-tool calls, timed individually)

1. `get_streaming_status` -- status read (minimal overhead)
2. `advance_step` -- streaming control (file I/O)
3. `get_available_arrays` -- pipeline query
4. `create_isosurface(V, 0.3)` -- filter creation
5. `get_screenshot` -- rendering + image transfer (skip for B3/B4)

### `scripted_complex` (multi-tool chain)

> "Pause the stream. Create an isosurface of V at 0.3. Color it by V. Rotate the camera 45 degrees. Take a screenshot. Then advance 5 steps, taking a screenshot after each."

Exercises ~12 tool calls: pause -> isosurface -> color_by -> rotate_camera -> screenshot -> (advance + screenshot) x 5.

### `scripted_headless` (for multi-node pvserver B3/B4)

Same as `scripted_basic` but omitting `get_screenshot` to avoid IceT compositing hang.

### Protocol

- Automated tests (A/B dimensions): `eval_harness.py` calls tools deterministically, 3 runs each
- Interactive tests: Claude Code calls tools via MCP, user drives the session
- Between runs: full pipeline teardown and re-launch
- Streaming bridge starts in `--paused` mode (agent controls advancement)

---

## 5. Implementation: Files to Create

| File | Purpose |
|------|---------|
| `test/insitu_agent/settings-staging-256.json` | L=256 simulation config (steps=200, plotgap=10, adios_config=adios2-sst.xml) |
| `test/insitu_agent/adios2-sst-blocking.xml` | SST variant with `QueueFullPolicy=Block` for throughput tests |
| `test/insitu_agent/eval_harness.py` | Deterministic MCP tool caller (no LLM) for automated benchmarking |
| `test/insitu_agent/run_scalability_eval.sh` | Master orchestration script -- iterates configs, launches components, collects results |
| `test/insitu_agent/collect_results.py` | Post-processing: merges timing JSONL files into consolidated CSV |

### `eval_harness.py` Design

```python
# Connects to MCP server via stdio (same pattern as insitu_agent.py)
# Calls tools in a fixed sequence -- no LLM, no variance
# Records: tool_name, args, start_time, end_time, duration_ms, result_summary

SCENARIOS = {
    "scripted_basic": [
        ("get_streaming_status", {}),
        ("advance_step", {}),
        ("get_available_arrays", {}),
        ("create_isosurface", {"value": 0.3, "field": "V"}),
        ("get_screenshot", {}),
    ],
    "scripted_headless": [
        ("get_streaming_status", {}),
        ("advance_step", {}),
        ("get_available_arrays", {}),
        ("create_isosurface", {"value": 0.3, "field": "V"}),
    ],
    "scripted_complex": [
        ("pause_streaming", {}),
        ("create_isosurface", {"value": 0.3, "field": "V"}),
        ("color_by", {"field": "V"}),
        ("rotate_camera", {"azimuth": 45.0}),
        ("get_screenshot", {}),
        # ... advance + screenshot x5
    ],
}
```

### `run_scalability_eval.sh` Pseudocode

```bash
# ---- Environment ----
eval $(spack load --sh adios2@2.11.0/g)
spack load paraview
export OMPI_MCA_pml=ob1
export OMPI_MCA_btl=tcp,self
export OMPI_MCA_osc='^ucx'
export OMPI_MCA_btl_tcp_if_include=eno1
export OMPI_MCA_oob_tcp_if_include=eno1

SSH_WRAPPER=~/software/gray-scott/ssh-spack-wrapper.sh
GS_EXE=~/software/gray-scott/build/adios2-gray-scott

for config in TEST_MATRIX; do
    # 0. Cleanup
    kill_all_components; rm -f gs.bp.sst

    # 1. Generate config files (settings JSON, ADIOS2 XML)
    # 2. Launch pvserver (background, on reader nodes via SSH wrapper)
    # 3. Launch simulation (background, on sim nodes via SSH wrapper)
    # 4. Wait for SST contact file (timeout 180s)
    # 5. Launch streaming bridge (background, on reader node 0)
    # 6. Run eval_harness.py (deterministic tool calls)
    # 7. Resume streaming to drain remaining steps
    # 8. Wait for simulation + bridge to finish (with watchdog kill)
    # 9. Kill pvserver
    # 10. Collect timing files -> results/run_${config_id}/
    # 11. Merge into results/scalability_eval.csv
done
```

---

## 6. Implementation: Files to Modify

### `insitu_mcp_server.py` -- Update ParaViewManager import path + add timing

**Import path change**: Use `~/software/paraview_mcp/paraview_manager.py` directly instead of the empty `paraview_mcp/` repo dir:

```python
PARAVIEW_MCP_DIR = Path.home() / "software" / "paraview_mcp"
sys.path.insert(0, str(PARAVIEW_MCP_DIR))
from paraview_manager import ParaViewManager
```

**Add MPI environment setup** (needed when MCP server runs under pvpython):

```python
import os
os.environ['OMPI_MCA_pml'] = 'ob1'
os.environ['OMPI_MCA_btl'] = 'tcp,self'
os.environ['OMPI_MCA_osc'] = '^ucx'
os.environ['OMPI_MCA_btl_tcp_if_include'] = 'eno1'
os.environ['OMPI_MCA_oob_tcp_if_include'] = 'eno1'
```

**Add two-level timing instrumentation**:

Level 1 (MCP total): Timing decorator on each `@mcp.tool()` function.
Level 2 (ParaView operation): Wrap each `pv_manager` call to capture just the ParaView time.

```python
import time, json, functools

TIMING_FILE = None

def timed_tool(func):
    """Decorator: measures total MCP tool time."""
    @functools.wraps(func)
    def wrapper(*args, **kwargs):
        t0 = time.monotonic()
        result = func(*args, **kwargs)
        mcp_total_ms = (time.monotonic() - t0) * 1000
        pv_op_ms = getattr(func, '_last_pv_ms', 0)
        if TIMING_FILE:
            with open(TIMING_FILE, "a") as f:
                json.dump({
                    "tool": func.__name__,
                    "pv_operation_ms": round(pv_op_ms, 2),
                    "mcp_total_ms": round(mcp_total_ms, 2),
                    "mcp_overhead_ms": round(mcp_total_ms - pv_op_ms, 2),
                    "timestamp": time.time(),
                }, f)
                f.write("\n")
        return result
    return wrapper

def _time_pv_call(tool_func, pv_method, *args, **kwargs):
    """Time a ParaViewManager method call and store on the tool function."""
    t0 = time.monotonic()
    result = pv_method(*args, **kwargs)
    tool_func._last_pv_ms = (time.monotonic() - t0) * 1000
    return result
```

Example usage in a tool:

```python
@mcp.tool()
@timed_tool
def create_isosurface(value: float, field: str = None) -> str:
    result = _time_pv_call(create_isosurface, pv_manager.create_isosurface, value, field)
    success, message, _, contour_name = result
    if success:
        return f"{message}. Filter registered as '{contour_name}'."
    return message
```

Add `--timing-file` argument to `main()`.

### `insitu_streaming.py` -- Add timing instrumentation

Add `--enable-timing` and `--timing-file` CLI flags. Instrument `streaming_loop()`:

```python
t_sst_poll_start = time.monotonic()
# ... PrepareNextStep polling loop ...
t_sst_poll_end = time.monotonic()

# ... UpdatePipelineInformation ...
t_pipeline_end = time.monotonic()

# ... Render(view) ...
t_render_end = time.monotonic()

write_timing_record({
    "step": state.step,
    "sst_wait_ms": (t_sst_poll_end - t_sst_poll_start) * 1000,
    "pipeline_update_ms": (t_pipeline_end - t_sst_poll_end) * 1000,
    "render_ms": (t_render_end - t_pipeline_end) * 1000,
    "step_total_ms": (t_render_end - t_sst_poll_start) * 1000,
})
```

**Add MPI environment setup** at script entry point:

```python
os.environ['OMPI_MCA_pml'] = 'ob1'
os.environ['OMPI_MCA_btl'] = 'tcp,self'
os.environ['OMPI_MCA_osc'] = '^ucx'
os.environ['OMPI_MCA_btl_tcp_if_include'] = 'eno1'
os.environ['OMPI_MCA_oob_tcp_if_include'] = 'eno1'
```

---

## 7. Known Risks & Mitigations

| Risk | Impact | Mitigation |
|------|--------|-----------|
| **IceT compositing hang** on multi-node `SaveScreenshot` | Blocks B3/B4 screenshot tests | Run B3/B4 with `scripted_headless`. Use B1/B2 for screenshot tests. Try `--force-offscreen-rendering`. |
| **SST QueueFullPolicy=Discard** drops steps when reader is slow | Throughput metrics skewed | Use `adios2-sst-blocking.xml` (Block policy) for throughput tests. Keep Discard for interactive agent sessions. |
| **Ares MPI environment volatile** | MPI launch failures, hangs, UCX errors | Always set OMPI_MCA env vars (pml=ob1, btl=tcp,self, osc=^ucx, btl_tcp_if_include=eno1, oob_tcp_if_include=eno1). Use SSH wrapper for cross-node launches. |
| **SST contact file path mismatch** | Reader can't find writer | Run simulation from `~/software/gray-scott/` so `gs.bp.sst` is created there. Use absolute paths for `-b` flag. |
| **Writer skips SST Close()** | Reader never gets EndOfStream | Use watchdog pattern from `run-sst.sh`: poll for SST contact file removal, grace period, then SIGTERM reader. |

---

## 8. Execution Phases

| Phase | Work | Duration |
|-------|------|----------|
| **Phase 1 - Setup & Validation** | Verify L=256 sim on 4-8 nodes. Verify pvserver + streaming bridge on 1 node. Update `insitu_mcp_server.py` import path to `~/software/paraview_mcp/`. Configure Claude Code MCP. Test MPI env vars. | ~1 day |
| **Phase 2 - Instrumentation** | Add timing to streaming bridge and MCP server. Add MPI env vars to Python scripts. Build `eval_harness.py` and `collect_results.py`. | ~1-2 days |
| **Phase 3 - Dimension A** | Sim scaling tests (A1-A3) with eval harness. | ~0.5 day |
| **Phase 4 - Dimension B** | Reader/pvserver scaling tests (B1-B4). | ~1 day |
| **Phase 5 - Interactive Agent** | Claude Code interactive sessions on B1/B2 configs. | ~0.5 day |
| **Phase 6 - Analysis** | Merge results, generate charts, write findings. | ~0.5 day |

---

## 9. Consolidated Output CSV Schema

```csv
config_id, sim_nodes, sim_nprocs, L, steps, output_steps,
reader_nodes, reader_nprocs,
sim_wall_s, sim_write_avg_ms,
sst_wait_avg_ms, pipeline_update_avg_ms, render_avg_ms,
step_throughput_steps_per_s,
pv_isosurface_avg_ms, pv_screenshot_avg_ms, pv_slice_avg_ms,
mcp_isosurface_avg_ms, mcp_screenshot_avg_ms, mcp_slice_avg_ms,
mcp_overhead_avg_ms,
total_wall_s
```

---

## 10. Expected Deliverables

| Deliverable | Description |
|-------------|-------------|
| `run_scalability_eval.sh` | Master orchestration script (with SSH wrapper, MPI env vars, watchdog) |
| `eval_harness.py` | Deterministic MCP tool caller for automated benchmarking |
| Instrumented `insitu_streaming.py` | Per-step SST/render timing + MPI env vars |
| Instrumented `insitu_mcp_server.py` | Per-tool timing + updated ParaViewManager import + MPI env vars |
| `settings-staging-256.json` | L=256 simulation config |
| `adios2-sst-blocking.xml` | Blocking SST config for throughput tests |
| `collect_results.py` | Merges timing sources into CSV |
| `results/scalability_eval.csv` | Master results file |
| `results/run_<config_id>/` | Per-run artifacts (logs, timing files, screenshots) |
| Claude Code MCP config | Updated `~/.claude/settings.json` for InSitu-ParaView |

---
---

# Evaluation Results (2026-04-05)

## Test Configuration

| Parameter | Value |
|-----------|-------|
| Cluster | Ares (ares-comp-XX, 16 cores/node) |
| Grid | L=256 (16.8M cells, ~268 MB/step for U+V float64) |
| Simulation | Gray-Scott, steps=200, plotgap=10, **20 SST output steps** |
| SST Config | Blocking, QueueLimit=5, WAN/TCP |
| Agent | Haiku (claude-haiku-4-5-20251001) via Anthropic SDK proxy |
| Agent scenario | scripted_basic: status → advance → arrays → isosurface(V,0.3) → screenshot → 5x(advance+screenshot) |

---

## Dimension A: Simulation Scaling

pvserver fixed at 1 process on localhost (ares-comp-21).

### Test Matrix (Executed)

| ID | Sim Nodes | nprocs | Wall Time |
|----|-----------|--------|-----------|
| A1 | 2 (ares-comp-25..26) | 32 | 209s |
| A2 | 4 (ares-comp-25..28) | 64 | 196s |
| A3 | 8 (ares-comp-25..29,17..19) | 128 | 233s |

### MCP Tool Timing (avg ms)

| Tool | A1 PV | A1 MCP | A2 PV | A2 MCP | A3 PV | A3 MCP |
|------|-------|--------|-------|--------|-------|--------|
| `get_streaming_status` | 0.0 | 1.2 | 0.0 | 1.2 | 0.0 | 1.2 |
| `advance_step` | 0.0 | 1.6 | 0.0 | 2.3 | 0.0 | 1.8 |
| `get_available_arrays` | 0.0 | 2795.2 | 0.0 | 2876.1 | 0.0 | 2815.4 |
| `create_isosurface` | 793.4 | 793.5 | 859.1 | 859.1 | 920.8 | 920.9 |
| `get_screenshot` | 274.5 | 274.7 | 300.0 | 300.2 | 303.0 | 303.1 |

> **Note**: `get_available_arrays` ~2800ms includes one-time lazy pvserver connect (~2.8s).
> MCP protocol overhead is <0.2ms on ParaView operations, <2ms on streaming control.

### Streaming Per-Step Timing (ms)

| Metric | A1 (2 nodes, 32p) | A2 (4 nodes, 64p) | A3 (8 nodes, 128p) |
|--------|-------------------|--------------------|--------------------|
| SST wait avg | 86.0 | 7.9 | 10.3 |
| SST wait max | 786.1 | 13.3 | 16.6 |
| Pipeline update avg | 76.5 | 77.0 | 82.1 |
| Render avg | 1461.0 | 1309.3 | 1356.9 |
| Render max | 2743.1 | 2319.5 | 1990.1 |
| **Step total avg** | **1623.5** | **1394.2** | **1449.3** |
| Step total max | 3529.6 | 2329.6 | 2135.0 |

### Per-Step Detail: A2 (4 nodes, 64 procs)

| Step | SST Wait (ms) | Pipeline (ms) | Render (ms) | Total (ms) |
|------|--------------|--------------|------------|-----------|
| 1 | 5.1 | 1533.6 | 439.4 | 1978.1 |
| 2 | 11.7 | 0.6 | 1716.0 | 1728.3 |
| 3 | 9.2 | 0.3 | 1701.5 | 1711.1 |
| 4 | 6.2 | 0.4 | 1617.1 | 1623.6 |
| 5 | 7.9 | 0.4 | 1609.8 | 1618.0 |
| 6 | 8.0 | 0.4 | 1352.1 | 1360.4 |
| 7 | 13.3 | 0.3 | 1270.6 | 1284.2 |
| 8 | 3.5 | 0.3 | 1220.6 | 1224.5 |
| 9 | 7.7 | 0.4 | 1401.2 | 1409.2 |
| 10 | 7.5 | 0.4 | 1158.2 | 1166.1 |
| 11 | 9.4 | 0.3 | 1790.1 | 1799.9 |
| 12 | 7.5 | 0.3 | 1962.8 | 1970.6 |
| 13 | 6.9 | 0.4 | 1501.7 | 1509.0 |
| 14 | 9.7 | 0.4 | 2319.5 | 2329.6 |
| 15 | 7.7 | 0.4 | 1018.0 | 1026.1 |
| 16 | 7.2 | 0.4 | 1119.7 | 1127.3 |
| 17 | 7.2 | 0.4 | 746.9 | 754.5 |
| 18 | 7.0 | 0.4 | 745.3 | 752.6 |
| 19 | 7.2 | 0.4 | 790.0 | 797.6 |
| 20 | 7.7 | 0.4 | 704.7 | 712.8 |

### Dimension A Analysis

1. **SST transfer cost is minimal**: avg 8-86ms/step. A1 higher (86ms avg, 786ms max) due to
   fewer writer procs producing data more slowly. A2/A3 SST wait is ~8-10ms.
2. **Rendering dominates step time**: 85-95% of per-step time is ParaView rendering (1.3-1.5s).
3. **Simulation scaling is flat on the reader side**: More sim nodes do not change
   reader/visualization time since data volume (L=256) is the same.
4. **MCP overhead is negligible**: <2ms per streaming control call, <0.2ms on PV ops.
5. **Pipeline update** is one-time ~1.5s (step 1 display setup), then <1ms thereafter.

---

## Dimension B: pvserver Scaling

Simulation fixed at 8 nodes, 128 procs, L=256.

### Test Matrix (Executed)

| ID | pvserver Procs | Nodes | Wall Time |
|----|---------------|-------|-----------|
| B1 | 1 | ares-comp-21 (local) | 264s |
| B_pv2 | 2 | ares-comp-21 (local) | 326s |
| B_pv4 | 4 | ares-comp-21 (local) | 312s |
| B_pv8 | 8 | ares-comp-21 (local) | 337s |
| B2 | 16 | ares-comp-21 (local) | 279s |
| B_pv32 | 32 | ares-comp-21 + ares-comp-22 (2 nodes) | 301s |
| B_pv64 | 64 | ares-comp-20..23 (4 nodes) | 354s |

### MCP Tool Timing (avg ms)

| Tool | 1p | 2p | 4p | 8p | 16p | 32p (2n) | 64p (4n) |
|------|-----|-----|-----|-----|------|----------|----------|
| `create_isosurface` PV | 915.7 | 791.1 | 656.9 | 620.4 | 527.0 | 618.3 | 547.5 |
| `create_isosurface` MCP | 915.7 | 791.1 | 657.0 | 620.5 | 527.1 | 618.4 | 547.6 |
| `get_screenshot` PV | 344.2 | 324.5 | 334.8 | 330.3 | 313.6 | 344.3 | 345.1 |
| `get_screenshot` MCP | 344.4 | 324.7 | 335.0 | 330.5 | 313.8 | 344.6 | 345.3 |

### Streaming Per-Step Timing (ms)

| Metric | 1p | 2p | 4p | 8p | 16p | 32p (2n) | 64p (4n) |
|--------|------|------|------|------|------|----------|----------|
| SST wait avg | 10.5 | 9.0 | 8.5 | 8.5 | 9.1 | 8.9 | 9.1 |
| Render avg | 1324.5 | 2534.9 | 2358.3 | 2283.2 | 2255.6 | 1153.6 | **612.5** |
| **Step total avg** | **1415.2** | **2693.2** | **2503.6** | **2425.1** | **2395.2** | **1256.0** | **670.6** |
| Render max | 2010.2 | 2698.6 | 2514.7 | 2406.5 | 2367.9 | 1204.7 | 633.9 |
| Step total max | 2190.7 | 3526.0 | 3259.1 | 3157.7 | 3200.0 | 2404.2 | 1541.8 |

### Per-Step Detail: B_pv32 (32 procs, 2 nodes cross-node)

| Step | SST Wait (ms) | Pipeline (ms) | Render (ms) | Total (ms) |
|------|--------------|--------------|------------|-----------|
| 1 | 3.3 | 1863.1 | 537.9 | 2404.2 |
| 2 | 19.7 | 0.5 | 1204.7 | 1224.9 |
| 3 | 8.7 | 0.4 | 1188.5 | 1197.6 |
| 4 | 8.2 | 0.3 | 1185.3 | 1193.8 |
| 5 | 8.3 | 0.3 | 1187.6 | 1196.1 |
| 6 | 9.9 | 0.2 | 1190.2 | 1200.4 |
| 7 | 9.9 | 0.2 | 1188.5 | 1198.7 |
| 8 | 8.0 | 0.2 | 1186.8 | 1195.1 |
| 9 | 7.9 | 0.3 | 1189.9 | 1198.1 |
| 10 | 8.8 | 0.3 | 1184.3 | 1193.4 |
| 11 | 8.3 | 0.3 | 1182.8 | 1191.5 |
| 12 | 10.3 | 0.3 | 1182.7 | 1193.4 |
| 13 | 8.3 | 0.2 | 1183.8 | 1192.3 |
| 14 | 7.9 | 0.3 | 1182.3 | 1190.5 |
| 15 | 8.2 | 0.2 | 1180.5 | 1188.9 |
| 16 | 8.6 | 0.4 | 1185.2 | 1194.1 |
| 17 | 8.2 | 0.2 | 1182.1 | 1190.5 |
| 18 | 8.2 | 0.2 | 1183.6 | 1192.0 |
| 19 | 8.2 | 0.2 | 1182.9 | 1191.3 |
| 20 | 9.7 | 0.2 | 1182.8 | 1192.7 |

### Per-Step Detail: B_pv64 (64 procs, 4 nodes)

| Step | SST Wait (ms) | Pipeline (ms) | Render (ms) | Total (ms) |
|------|--------------|--------------|------------|-----------|
| 1 | 3.4 | 974.4 | 564.0 | 1541.8 |
| 2 | 19.6 | 0.5 | 627.1 | 647.2 |
| 3 | 9.0 | 0.2 | 612.4 | 621.7 |
| 4 | 8.7 | 0.2 | 614.0 | 622.9 |
| 5 | 9.7 | 0.4 | 633.9 | 643.9 |
| 6 | 8.4 | 0.2 | 616.7 | 625.3 |
| 7 | 10.0 | 0.3 | 613.9 | 624.2 |
| 8 | 8.4 | 0.2 | 612.3 | 621.0 |
| 9 | 8.4 | 0.2 | 614.5 | 623.1 |
| 10 | 10.3 | 0.2 | 615.4 | 626.0 |
| 11 | 8.4 | 0.2 | 613.9 | 622.5 |
| 12 | 8.8 | 0.3 | 613.6 | 622.8 |
| 13 | 8.4 | 0.2 | 610.2 | 618.9 |
| 14 | 8.3 | 0.2 | 612.2 | 620.7 |
| 15 | 8.5 | 0.2 | 612.0 | 620.7 |
| 16 | 8.8 | 0.3 | 614.3 | 623.4 |
| 17 | 8.5 | 0.2 | 611.0 | 619.7 |
| 18 | 8.7 | 0.4 | 611.0 | 620.0 |
| 19 | 8.1 | 0.2 | 612.9 | 621.1 |
| 20 | 9.9 | 0.2 | 615.1 | 625.2 |

### Dimension B Analysis

1. **Isosurface creation scales with procs**: 916ms (1p) → 527ms (16p) → 548ms (64p, 4 nodes).
   ~1.7x speedup, plateauing beyond 16 procs.
2. **Screenshot time is roughly constant** (~314-345ms) regardless of pvserver parallelism.
3. **Per-step render gets SLOWER with 2-16 local procs** (1325ms → 2256-2535ms) due to
   IceT compositing overhead and CPU contention on a single node.
4. **Cross-node pvserver scales well**:
   - 32 procs (2 nodes): 1154ms/step — faster than single-proc (1325ms)
   - **64 procs (4 nodes): 613ms/step — 2.2x faster than single-proc**
   - Distributing MPI ranks across nodes eliminates single-node contention.
5. **64-proc render is extremely stable**: steps 2-20 all within 610-634ms (σ < 7ms).
6. **SST transfer is unaffected** by pvserver parallelism (~9ms in all configs).

---

## Cross-Node pvserver Diagnosis

### Initial Failure

The first attempt to run pvserver on a remote node (ares-comp-22) failed with 60s
timeouts on all ParaView operations.

**Root cause**: The SSH wrapper for mpirun (`ssh-spack-wrapper.sh`) only injects the
ADIOS2 spack PATH, not ParaView's. When `prterun` tried to launch `pvserver` on the
remote node via the wrapper, it could not find the executable in PATH.

Error message:
```
prterun was unable to find the specified executable file
Node: ares-comp-22
Executable: .../paraview-5.13.3-.../bin/pvserver
```

### Fix

Changed the mpirun launch to use `bash -c "spack load paraview && pvserver ..."`
instead of invoking `pvserver` directly. This ensures each MPI rank loads the
ParaView spack environment before starting pvserver.

### Verification

After the fix, 32-proc pvserver across 2 nodes (ares-comp-21 + ares-comp-22) works:
- Connect: 2.6s (MPI barrier + client-server handshake)
- Render (Sphere test): 1.4s (IceT cross-node compositing OK)
- SaveScreenshot: 44ms (IceT compositing over TCP works)
- Full eval run: 301s wall time, all 20 steps processed, agent exit 0

**IceT compositing over TCP works on Ares** when the MPI environment is properly
configured (OMPI_MCA pml=ob1, btl=tcp,self, btl_tcp_if_include=eno1). The earlier
assumption that cross-node pvserver would fail due to IceT hangs was incorrect —
the issue was purely a PATH/environment problem in the SSH wrapper.

---

## Overall Summary

### Time Breakdown by Layer

| Layer | 1 pvserver proc | 64 procs (4 nodes) | Speedup |
|-------|----------------|-------------------|---------|
| SST data transfer | 10 ms | 9 ms | 1.0x |
| Pipeline update | <1 ms | <1 ms | 1.0x |
| ParaView render | 1,325 ms | 613 ms | **2.2x** |
| **Step total** | **1,415 ms** | **671 ms** | **2.1x** |
| MCP overhead | <2 ms | <2 ms | — |
| Isosurface (tool call) | 916 ms | 548 ms | **1.7x** |
| Screenshot (tool call) | 344 ms | 345 ms | 1.0x |

### pvserver Scaling Curve

```
Step total avg (ms) vs pvserver procs:

3000 |     x
     |       x
2500 |         x
     |           x
2000 |             x
     |
1500 | x                           (single-node contention zone)
     |                               -------------------------
1000 |                         x              x
     |                                          
 500 |                                                  x
     +----+----+----+----+-----+-----+------+------+
     0    2    4    8    16    32    64   procs
                              (multi-node)
```

Single-node (2-16p): render is SLOWER due to IceT compositing + CPU contention.
Multi-node (32-64p): render scales well — distributing across nodes eliminates contention.

### Key Takeaways

1. **Rendering is the bottleneck** (90-95% of per-step time), not data transfer or MCP.
2. **MCP adds <2ms overhead** — the protocol is essentially free.
3. **Simulation scaling (Dim A)** has minimal impact on reader/visualization time.
4. **pvserver parallelism (Dim B)** shows a clear single-node vs multi-node pattern:
   - **Single-node (2-16 procs)**: SLOWER than 1 proc due to IceT compositing overhead
     and CPU/memory contention. Worst at 2 procs (2693ms, 1.9x slower).
   - **Multi-node (32-64 procs)**: FASTER than 1 proc. 64 procs on 4 nodes achieves
     **2.2x speedup** (613ms vs 1325ms render) with very stable per-step timing.
5. **Cross-node pvserver works** when MPI environment is properly configured.
   The earlier failure was a PATH issue, not an IceT/network limitation.
6. **Optimal config for L=256**: 4 pvserver nodes (64 procs) gives best throughput.
   For larger grids, more nodes would likely show even better scaling.
