# Agent vs Baseline Visualization: ParaView (Remote/Local) vs LLM Agent

## Motivation

The previous scalability eval (`SCALABILITY_EVAL_PLAN.md`) measured *infrastructure* performance. This new evaluation compares two **visualization workflows** for in-situ Gray-Scott analysis:

| Workflow | What it does | Decision maker |
|----------|-------------|----------------|
| **Baseline (ParaView)** | Render every K-th SST step with a fixed pipeline | Fixed schedule |
| **Agent-based** | LLM picks which steps to visualize and what to look at | Claude (Haiku/Sonnet/Opus) |

The **baseline uses local rendering** for this initial evaluation:
- **Local rendering** (Phase 1) — `pvpython` does both data ingestion *and* rendering in the same process. No client-server split, no IceT compositing, no network-shipped framebuffers.
- **Remote rendering** (Phase 2, deferred) — `pvserver` runs as a separate MPI job; the bridge connects via TCP. To be added in a follow-up evaluation.

The question: **does an LLM agent visualize the *right* moments more efficiently than a fixed ParaView baseline, and at what token cost?**

---

## Background: Sim vs Viz Time Budget

### Pure Simulation Time (measured 2026-04-05)

Measured by running gray-scott alone, no reader, 2000 steps, L=256, 4 nodes (64 procs):

| Metric | Value |
|--------|-------|
| Total wall time (2000 steps) | **264.6 s** |
| Per simulation step | **132 ms** |
| Per output @ plotgap=10 | **1.32 s** |
| Per output @ plotgap=20 | **2.65 s** |

### Visualization Time (from previous scaling eval)

| Render config | Render time/step |
|--------------|------------------|
| Remote pvserver, 1 proc | 1.33 s |
| Remote pvserver, 64 procs (4 nodes) | 0.61 s |
| Local pvpython (in-process) | TBD |

### Sim vs Viz Bottleneck

With **blocking SST**, the slower of the two dominates the wall time. The earlier
"127x slower viz" claim was based on a misleading metric (SST wait time, which is
not the same as sim_per_output). The truth is:

| plotgap | Sim/output | Viz (B_pv64) | Viz (B1) | Bottleneck |
|---------|-----------|--------------|----------|------------|
| 5 | 0.66 s | 0.61 s | 1.33 s | Balanced (B_pv64) / Viz (B1) |
| 10 | 1.32 s | 0.61 s | 1.33 s | Sim (B_pv64) / Balanced (B1) |
| **20** | **2.65 s** | **0.61 s** | **1.33 s** | **SIM dominates both** |
| 50 | 6.62 s | 0.61 s | 1.33 s | Sim |

### Optimal plotgap: `render_time / sim_per_step`

To balance sim and viz so neither blocks the other:
- **B1 (1 proc remote)**: optimal plotgap ≈ 1330 / 132 = **10**
- **B_pv64 (4 nodes remote)**: optimal plotgap ≈ 610 / 132 = **5**
- **Local rendering**: TBD (need to measure local render time first)

### Why plotgap=20 is the Right Choice for This Eval

With **plotgap=20, the simulation is the bottleneck** (2.65 s/output > 1.33 s viz).
This means:
1. **Viz pipeline does NOT slow the simulation** — viz can always keep up
2. **All trigger strategies (K=1, K=2, K=5, agent) have similar wall time** (~5 min total),
   dominated by sim
3. **The comparison becomes purely about *what* is visualized**, not how fast it renders
4. This is a **fair eval setup** — we isolate trigger strategy from runtime confounds

**Trade-off**: at plotgap=20 with K=1, we generate 100 visualizations — a lot for an
agent to reason over (token cost ~$0.75/Sonnet run, $0.20/Haiku run).
At plotgap=10 with K=1, we'd have 200 visualizations — even more, plus sim and viz
become balanced (slight viz blocking).

---

## Two Workflows

### Workflow 1: Baseline ParaView

**Mechanism**: visualize every K SST output steps with a fixed pipeline (isosurface(V, 0.3) → screenshot). No skipping, no thinking, no LLM.

**K from literature** (Gray-Scott pattern formation):
- Karch et al., Pugmire et al. (in-situ Catalyst): every 5-10 sim iterations
- With our `plotgap=20`, that maps to **K=1 SST step** (every output) for fine-grained,
  K=2-5 for coarser tracking

**K from infrastructure ratio** (no-block constraint):
- We deliberately chose plotgap=20 so the **simulation is the bottleneck** (2.65s sim/output
  vs ~1.3s render). This means **viz does NOT block the sim** — every K is feasible without
  slowing the run.
- Wall time is ~constant across K values (~5 minutes, dominated by sim).

**Rendering mode (Phase 1)**: **Local rendering only**
- A single `pvpython` process does *everything*: SST read, Fides, render, screenshot
- No `Connect()` call, no separate pvserver
- Native ParaView rendering in-process (offscreen via OSMesa or EGL)
- No IceT (single rank), no network framebuffer ship-back
- Expected: **lower latency per render** vs remote, **no scaling beyond 1 proc**

**Remote rendering** (Phase 2, deferred): will be added in a follow-up evaluation, comparing
single-proc vs multi-proc pvserver vs local rendering at the same K.

**Phase 1 test variants** (local rendering, **2000 sim steps, plotgap=20 → 100 SST outputs**):
| ID | Rendering | K | Total Vis |
|----|-----------|---|-----------|
| BL-Local-K1 | Local (in-process) | 1 | 100 |
| BL-Local-K2 | Local (in-process) | 2 | 50 |
| BL-Local-K5 | Local (in-process) | 5 | 20 |
| BL-Local-K10 | Local (in-process) | 10 | 10 |

**Visualization content**: fixed pipeline — `isosurface(V, 0.3)` + `screenshot`. No coloring/camera changes.

**Pros**: simple, predictable, zero LLM cost.
**Cons**: blind to interesting events; either over-samples or misses transitions.

---

### Workflow 2: Agent-Based

**Mechanism**: an LLM (Haiku or Sonnet) decides each step whether to advance, pause, visualize, change camera, or take a screenshot. The agent can vary the visualization mid-run (different isovalues, slices, camera angles, color maps).

**Tool budget**: all 24 MCP tools available. The agent runs in a loop until the stream ends or a token budget is hit.

**Two prompting modes**:

**Mode A: Open-ended exploration**
> "Explore this Gray-Scott simulation. Take screenshots when you observe interesting
> pattern transitions. Decide when to advance steps and when to pause for inspection.
> Report what you see."

**Mode B: Goal-directed**
> "Find the timestep where the spot patterns first emerge. Take screenshots only when
> you see significant changes. Justify each screenshot."

**Test variants** (Haiku and Sonnet only — both fast models, dropping Opus for cost/speed):
| ID | Model | Mode | Notes |
|----|-------|------|-------|
| AG-Haiku-Open | claude-haiku-4-5 | A | Cheapest, exploratory |
| AG-Haiku-Goal | claude-haiku-4-5 | B | Cheapest, focused |
| AG-Sonnet-Open | claude-sonnet-4-5 | A | Stronger reasoning, exploratory |
| AG-Sonnet-Goal | claude-sonnet-4-5 | B | Stronger reasoning, focused |

The agent uses **local rendering** in Phase 1 (same as the baseline) so the comparison is
purely about *trigger strategy*, not rendering speed.

**API endpoint** (proxy):
```bash
export ANTHROPIC_BASE_URL="https://yxai.anthropic.edu.pl"
export ANTHROPIC_API_KEY="sk-4TbsHfCmmzbsvw4Ynzk2tiZDWMIv1jwtL1x94ELsgnxYUDHR"
```

**Pros**: adaptive, can describe what it sees, can change viz pipeline mid-run.
**Cons**: token cost; non-deterministic; quality depends on prompt + model.

---

## Metrics

For each run, capture:

| Metric | Type | Notes |
|--------|------|-------|
| `num_visualizations` | count | Screenshots actually saved |
| `num_advance_calls` | count | SST steps consumed |
| `wall_time_s` | time | Total run wall time |
| `vis_wall_time_s` | time | Time spent rendering + saving images |
| `decision_time_ms_per_step` | time | Time deciding (rule eval / LLM call) |
| `coverage_ratio` | float | num_visualizations / 20 (output steps) |
| `total_image_bytes` | bytes | Sum of screenshot sizes |
| `input_tokens` | count | LLM input tokens (agent only) |
| `output_tokens` | count | LLM output tokens (agent only) |
| `image_input_tokens` | count | Tokens for images sent back to LLM (agent only) |
| `total_token_cost_usd` | $ | Anthropic pricing applied |
| `key_events_captured` | qualitative | Did it catch interesting transitions? |

Plus from the existing instrumentation:
- `streaming_timing.jsonl` — per-step SST/pipeline/render times
- `mcp_tool_timing.jsonl` — per-tool MCP/PV timings (agent only)

---

## Token Cost Modeling

### Anthropic Pricing (April 2026, per million tokens)

| Model | Input | Output |
|-------|-------|--------|
| Haiku 4.5 | $1.00 | $5.00 |
| Sonnet 4.5 | $3.00 | $15.00 |

### Image Token Cost

Anthropic Vision charges input tokens per image based on dimensions:
**tokens ≈ (width × height) / 750**

Our screenshots: 400×400 → ~213 input tokens, plus fixed overhead.
**Estimate: ~250 input tokens per screenshot.**

### Estimated Cost: 100-output run

The agent does up to 100 advance + a variable number of screenshots (typically 20-50).
Each tool call sends back the cumulative conversation context.

Conservative estimate for **Sonnet, 100 advances + ~30 screenshots**:
- Per-tool-call context (system prompt + tool schemas + history): ~3 KB ≈ 750 input tokens
- 100 calls × 750 = 75,000 input tokens
- Image tokens: 30 × 250 = 7,500 input tokens
- Output tokens: 100 × 200 = 20,000 (mostly short tool-use messages)
- **Sonnet cost**: 82,500 × $3/M + 20,000 × $15/M = **$0.25 + $0.30 = $0.55**
- **Haiku cost**: 82,500 × $1/M + 20,000 × $5/M = **$0.08 + $0.10 = $0.18**

Per-model estimates for 100-step runs:
| Model | Estimated cost |
|-------|---------------|
| Haiku | ~$0.20 |
| Sonnet | ~$0.70 |

---

## Test Matrix (Phase 1: local rendering only)

**Sim config**: 4 nodes (64 procs), L=256, **2000 sim steps, plotgap=20 → 100 SST output steps**.
All runs use **local pvpython rendering** — no separate pvserver, no IceT, no client-server split.

**Why 2000 steps / plotgap=20?**
- 2000 steps × dt=2.0 = 4000 time units → enough for full Gray-Scott pattern formation
- plotgap=20 puts the **sim as bottleneck** so viz doesn't slow it down (fair comparison)
- 100 SST outputs gives the agent meaningful decision diversity
- ~5 min wall time per run

| ID | Workflow | Variant | Max Vis Count | Est Cost |
|----|----------|---------|---------------|----------|
| BL-Local-K1 | Baseline | Local, every SST step | 100 | $0 |
| BL-Local-K2 | Baseline | Local, every 2 SST steps | 50 | $0 |
| BL-Local-K5 | Baseline | Local, every 5 SST steps | 20 | $0 |
| BL-Local-K10 | Baseline | Local, every 10 SST steps | 10 | $0 |
| AG-Haiku-Open | Agent | Haiku, open-ended exploration | ≤100 | ~$0.20 |
| AG-Haiku-Goal | Agent | Haiku, goal-directed | ≤100 | ~$0.15 |
| AG-Sonnet-Open | Agent | Sonnet, open-ended exploration | ≤100 | ~$0.80 |
| AG-Sonnet-Goal | Agent | Sonnet, goal-directed | ≤100 | ~$0.70 |

**8 runs total**, ~5-7 minutes each. Total wall ~1 hour, total token cost ~$2.

**Phase 2 (deferred)**: add remote-rendering baseline variants and re-run agent with remote.

---

## Implementation Plan

### Step 1: Add local-rendering bridge variant

The current `insitu_streaming.py` always calls `Connect()` → connects to a remote pvserver.
Add a `--local-render` flag that:
- Skips `Connect()`
- Creates the render view in-process
- Uses offscreen rendering (OSMesa / EGL)
- Calls `SaveScreenshot` directly to disk
- Keeps all existing timing instrumentation

This is the foundation for the baseline runs *and* the agent runs in Phase 1.

### Step 2: Baseline runner (no LLM)

Create `run_baseline.py` — a non-LLM Python script that:
1. Either uses MCP `advance_step` + `create_isosurface` + `get_screenshot`, OR drives
   the local-render bridge directly via `paraview.simple`
2. Loop: advance → if `step % K == 0` → render + screenshot
3. Records timing, image bytes, decision count to `baseline_metrics.jsonl`
4. Exits when stream ends

### Step 3: Agent runner enhancements

Modify existing `insitu_agent.py` to:
- **Capture token usage**: the Anthropic SDK returns `response.usage` with
  `input_tokens`, `output_tokens`, and `cache_read_input_tokens`. Sum across the run.
- **Track image bytes**: count screenshots and total bytes from MCP `get_screenshot` results.
- **Log decisions**: write `{tool, args, result_summary, timestamp}` to `decisions.jsonl`.
- **Save final summary**: write `{total_input_tokens, total_output_tokens, total_image_tokens,
  num_screenshots, num_advances, total_cost_usd}` to `agent_summary.json`.

### Step 4: Comparison harness

Create `run_workflow_comparison.sh`:
1. Set up sim + bridge (with `--local-render`) once
2. For each variant: run the appropriate runner, collect metrics
3. Reset stream between runs (re-launch sim)
4. Generate per-run JSON + final comparison CSV

### Step 5: Analysis

Generate `WORKFLOW_COMPARISON.md` with:
- Side-by-side metric tables (baseline K1/K2/K5 vs Haiku vs Sonnet)
- Token cost breakdown per agent run
- Image timeline: which timesteps each variant visualized
- Cost-per-visualization analysis
- Qualitative comparison: do agent screenshots catch transitions baseline misses?

### Phase 2 (deferred): Remote rendering

After Phase 1 is complete, add remote-rendering variants:
- `BL-Remote-1p-K1`, `BL-Remote-64p-K1`, `BL-Remote-64p-K2`
- `AG-Haiku-Goal-Remote`, `AG-Sonnet-Goal-Remote`
Compare against the Phase 1 local-rendering numbers.

---

## Files to Create / Modify

| File | Purpose |
|------|---------|
| `insitu_streaming.py` | **Modify**: add `--local-render` flag |
| `insitu_agent.py` | **Modify**: add token usage capture + summary JSON |
| `run_baseline.py` | **New**: fixed-period visualization runner (no LLM) |
| `run_workflow_comparison.sh` | **New**: master orchestration |
| `analyze_workflows.py` | **New**: compute metrics, generate report |
| `prompts/agent_open.txt` | **New**: open-ended exploration prompt |
| `prompts/agent_goal.txt` | **New**: goal-directed prompt |
| `results/workflow_comparison/` | Per-run artifacts |
| `WORKFLOW_COMPARISON.md` | Final results report |

---

## Open Questions

1. **Ground truth**: how do we judge "captured the right moments"?
   - Manual labeling: human marks key timesteps from a full 20-step run
   - Quantitative: image entropy diff between consecutive frames
   - Hybrid: agent picks N steps; check overlap with human-labeled key steps
2. **Token budget cap**: should we cap the agent at e.g. 50K input tokens to force economy?
   Or unbounded?
3. **Same sim or different seeds**: same sim is fairer for direct comparison; different seeds
   tests robustness.
4. **L=512 follow-up**: should we also test with larger data to see how cost ratios shift?

---

## Success Criteria (Phase 1)

We can answer:

1. **Baseline vs agent count**: how many fewer visualizations does the agent produce
   compared to fixed K=1, K=2, K=5?
2. **Coverage vs cost trade-off**: cost per "useful" screenshot for Haiku vs Sonnet.
3. **Model tier**: does Sonnet pick meaningfully better moments than Haiku, or is the
   cheap model good enough for Gray-Scott?
4. **Local rendering performance**: how does single-proc local rendering compare to
   the previously measured single-proc remote rendering (B1: 1325 ms/step render)?

## Phase 2 Success Criteria (deferred)

After remote rendering is added back:

5. **Local vs remote rendering**: which is faster at K=1? Does IceT compositing
   overhead of remote pvserver actually beat single-proc local?
6. **Does the agent benefit from faster rendering?** Compare Sonnet on local vs remote.
