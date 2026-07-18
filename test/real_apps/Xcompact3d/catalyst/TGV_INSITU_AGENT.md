# In-situ AI agent for the Xcompact3d TGV stream

The gray-scott interactive agent stack (`test/insitu_agent/`) ported to the
Xcompact3d TGV Catalyst SST stream - verified end-to-end (including the live
LLM agent, 2026-07-12) on Ares against the trigger-gated 25-rank 65³ Re=5000
run (writer ares-comp-18/20/22/23, consumer ares-comp-26).

```
Xcompact3d (hermes engine, gated SST "tgv.bp")
        │  ships only the Red inspect-window steps
        ▼
tgv_insitu_streaming.py  (pvpython bridge, consumer node)
        │  Fides/SST → pvserver; resamples 25 pencil blocks to one 65³
        │  image and volume-renders vort; saves tgv_bridge_view.png/step
        ▼
pvserver --multi-clients :11112  ◄──  tgv_insitu_mcp_server.py (24 MCP tools)
                                              ▲ stdio
                                     insitu_agent.py (LLM loop)
```

## Files (this directory)

| File | Role |
|------|------|
| `tgv_insitu_streaming.py` | streaming bridge; TGV port of `insitu_agent/insitu_streaming.py` (`--field vort` default; `TGV_BRIDGE_RESAMPLE=0` disables the seamless-volume resample) |
| `tgv_insitu_mcp_server.py` | MCP server; TGV prompt/field docs, same 24 tools + dedicated-view empty-screenshot fix |
| `tgv_agent_system_prompt.txt` | agent system prompt (pass via `insitu_agent.py --system-prompt-file`) |
| `run_tgv_consumer.sh` | starts pvserver + bridge on the consumer node |
| `tgv-fides.json` | Fides data model (ux/uy/uz/vort/critq, no step_information) |
| `tgv-pipeline.py` | non-interactive pvbatch consumer (fixed slice render, exits after N steps) |

The agent driver is shared: `test/insitu_agent/insitu_agent.py` grew
`--mcp-server-script` and `--system-prompt-file` so it serves both sims.

## How to run (verified sequence)

Writer (pipeline `coeus-xcompact3d`, trigger on):

```bash
jarvis pkg configure coeus-xcompact3d.jarvis_coeus.Incompact3d trigger=true
rm -f /mnt/common/hxu40/incompact3d/output/tgv.bp.sst
jarvis ppl kill; jarvis ppl run          # contact file appears ~30 s in
```

Consumer (ares-comp-26) - after (or before) the contact file appears:

```bash
ssh ares-comp-26 bash <this-dir>/run_tgv_consumer.sh
# env overrides: OUTPUT_DIR, PVSERVER_PORT (11112), BRIDGE_ARGS
```

The bridge starts `--paused` at step 0. In gated mode nothing arrives until
the Red fire (~output 39, t=3.9, ≈7 min for the 65³ Re=5000 case); then each
`advance_step` pulls one inspect-window step (arrives in 1–3 s).

Agent (run on the consumer node - see NFS note below):

```bash
PV=/mnt/common/hxu40/spack/opt/spack/linux-skylake_avx512/paraview-5.13.3-ssmv5hp4czyfvuu5eps6s2ljpug7lkus
PY=/mnt/common/hxu40/spack/opt/spack/linux-skylake_avx512/python-3.12.12-mmtjcqm5y4jajxdmcpgbzbnocjfk4afq/bin/python3
export ANTHROPIC_API_KEY=$(cat ~/.anthropic_key)   # official sk-ant-... API key
CAT=<this-dir>
PYTHONPATH="$PV/lib/python3.12/site-packages:/mnt/common/hxu40/software/paraview_mcp" \
LD_LIBRARY_PATH="$PV/lib" \
$PY /mnt/common/hxu40/coeus/iowarp/coeus-adapter/test/insitu_agent/insitu_agent.py \
    --provider anthropic --model claude-haiku-4-5-20251001 \
    --mcp-server-script $CAT/tgv_insitu_mcp_server.py \
    --system-prompt-file $CAT/tgv_agent_system_prompt.txt \
    --server-host localhost --server-port 11112 \
    --status-file $CAT/tgv_streaming_status.json \
    --screenshot-file $CAT/tgv_bridge_view.png \
    --results-dir <run-dir> \
    --prompt "A numerical-dissipation trigger just fired (Red: the
              under-resolved vortex cascade has begun). The stream is paused
              before the first inspect-window step. Advance through the 3
              inspect-window steps one at a time: after each advance_step,
              check get_streaming_status until the step counter increases,
              then take a screenshot and describe the vortex structures.
              After the 3rd step, create a vort isosurface at a value you
              judge appropriate from what you saw, screenshot it, and
              summarize how the cascade evolved across the window."
```

Telling the agent to *poll `get_streaming_status` until the step counter
increases* is what makes gated stepping robust: `advance_step` only returns
"command sent", and in gated mode the next step may not exist yet, so the
agent must confirm arrival before it screenshots (otherwise it captures the
previous frame). The 2026-07-12 run below used exactly this prompt.

## What we observed - live LLM agent run (2026-07-12)

Full workflow end to end: `jarvis ppl run` (gated writer, 25 ranks / 4 nodes,
65³ Re=5000 dt=0.005 io=20) → `run_tgv_consumer.sh` on ares-comp-26 →
`insitu_agent.py --provider anthropic --model claude-haiku-4-5-20251001`
(official Anthropic API) with the single-shot prompt above. Artifacts of
record: `/mnt/common/hxu40/incompact3d/output/agent_run_llm/`
(`screenshots/agent_0001..0004.png`, `token_usage.json`); the writer's
per-step diagnostics are in `.../output/trigger_metrics.jsonl` and the
engine trigger log is `.../output/logs/engine_test_<rand>.txt`.

### 1. The trigger fired on schedule (writer side)

The engine pools the derived block means every output step and evaluates the
two-stage Yellow/Red dissipation trigger. It reproduced the canonical
timeline exactly (identical to the 07-09/07-10 runs - the pooled metrics are
decomposition-deterministic):

| output | sim time | eps_frac | nu_ratio | trigger |
|-------:|---------:|---------:|---------:|---------|
| 2  | t=0.2 | 0.0827 | 1.090 | Yellow (transient; log-only) |
| …  | …     | 0 (guarded) | - | quiet - guards hold eps_frac=0 while TKE is not decaying |
| 38 | t=3.8 | 0.1229 | 1.140 | Yellow |
| **39** | **t=3.9** | **0.3520** | **1.543** | **RED → opens 3-step SST window** |

Red at output 39 means the numerical (non-physical) share of the dissipation
jumped to 35 % and the effective viscosity to 1.54× molecular - the
under-resolved cascade onset. Only from here does the writer ship anything.

### 2. The gate is agent-paced, not wall-clock (writer side)

The engine logs how long each flagged step took to hand off over SST
(`QueueLimit=1`, so the writer blocks at `EndStep` until the reader drains
the previous step). Because the agent was driving `advance_step` between its
LLM turns, the middle step sat in the engine until the model asked for it:

| flagged step | ship time | what gated it |
|-------------:|----------:|---------------|
| 39 | 0.24 s | reader already waiting at the fire |
| **40** | **33.7 s** | writer blocked in `EndStep` - the LLM was reasoning/screenshotting step 39 |
| 41 | 0.33 s | agent had advanced again |

The 33.7 s hold is the load-bearing observation: the simulation's output
cadence was literally paced by the agent's decisions, which is the whole
point of trigger-gated in-situ steering - the writer only spends bandwidth on
steps the consumer actually asks to see, and waits (rather than dropping or
racing ahead) while the consumer thinks.

### 3. What the agent did (agent side)

19 LLM calls / 18 tool calls / 4 screenshots in 46 s. Unassisted, it:

1. Checked `get_streaming_status`, saw paused at step 0.
2. For each of the 3 window steps: `advance_step`, then **polled
   `get_streaming_status` until the counter incremented** (correctly handling
   that the gated step isn't instantaneous), then `get_screenshot` and a
   written description of the vortex field.
3. After step 3, chose to build a `create_isosurface(field=vort, value=2.5)`
   - a value it picked from the volume renders it had seen - and screenshotted
   it.
4. Produced a physically correct cascade narrative, e.g. at step 3:
   *"The primary filament structure has fragmented further - there are now
   distributed patches of orange/red high-vorticity regions scattered
   throughout the volume rather than concentrated tubes… the small-scale
   cascade is evident in the grainy texture,"* and closed with a
   forward-energy-cascade summary (coherent TGV filaments → fragmenting tubes
   → multi-scale vortex-sheet network).

### 4. The rendered frames (`screenshots/`)

- `agent_0001..0003.png` - the bridge's seamless 65³-resampled volume
  rendering of `vort` at window steps 1–3 (the raw stream is 25 pencil
  blocks; the bridge's `ResampleToImage` removes the block-seam slabs).
- `agent_0004.png` - the agent's own `vort=2.5` isosurface, rendered in the
  isolated MCP view. It is non-empty and shows the fragmented sheet/filament
  network, confirming the empty-screenshot fix holds when the agent adds its
  own filter into a shared collaboration session.

### 5. Run economics

| metric | value |
|--------|------:|
| model | claude-haiku-4-5-20251001 |
| LLM calls / tool calls / screenshots | 19 / 18 / 4 |
| input / output tokens | 120,111 / 1,679 |
| wall time | 46.2 s |
| estimated cost | ~$0.13 |

Consistent with the gray-scott Haiku agent runs (~$0.11, 25 tool calls).

### Prior validation (2026-07-11)

- Ungated smoke test: bridge served steps on `advance_one`, volume-rendered
  vort, status/command protocol worked, MCP screenshots (bridge PNG +
  isolated MCP view) returned, `vort=1.2` isosurface rendered; bridge renders
  stayed non-empty after MCP filters.
- Gated run driven by a *scripted* MCP client (before a valid key was
  available): same Red-at-39 fire, 3× `advance_step` → screenshot →
  `create_isosurface(vort=8)` + `color_by`. Artifacts:
  `/mnt/common/hxu40/incompact3d/output/gated_agent_test/`.

## Gotchas

- **Run MCP server/agent on the consumer node.** The bridge↔MCP command
  protocol is file-based; across NFS the bridge can take 30–60 s to see a
  command written from another node (attribute-cache latency). Same node =
  sub-second.
- pvserver port 11112 (11111 conflicts on Ares); `--multi-clients` required.
- MCP server needs spack python3.12 (NOT pvpython) with
  `PYTHONPATH=<paraview site-packages>:~/software/paraview_mcp` and
  `LD_LIBRARY_PATH=<paraview lib>`.
- Absolute paths for the bridge's `-j`/`-b` args.
- Early-TGV vort max ≈ 2 (isosurface 2.0 is empty); window steps at the fire
  have vort max 13–20 (isosurface 8 is good).
- pv_manager's `set_representation_type`/`color_by` act on ITS active source,
  which may be the Fides reader rather than the last contour - the agent
  sees the reader's block surfaces and should `toggle_visibility` it off
  (interactive agents recover from the screenshot feedback; same behavior
  as gray-scott).
- `jarvis ppl kill` + `pkill xcompact3d` + remove `tgv.bp.sst` before reruns.
