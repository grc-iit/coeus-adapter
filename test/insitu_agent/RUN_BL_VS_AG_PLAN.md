# Run Plan: Baseline vs Agent Visualization Comparison

## Goal

Run **Baseline (BL)** and **Agent (AG)** workflows on identical Gray-Scott simulation
configuration to compare visualization trigger strategies. Results go into a new
isolated folder so they don't mix with the previous scaling-eval data.

## Simulation Configuration (fixed for all runs)

| Parameter | Value |
|-----------|-------|
| L (grid edge) | 256 |
| Total sim steps | 2000 |
| plotgap | 20 |
| **SST output steps** | **100** |
| dt | 2.0 |
| Total time units | 4000 |
| Sim procs | 128 |
| Sim nodes | 8 (ares-comp-10..17) |
| ppn | 16 |
| SST mode | Blocking, QueueLimit=5 |

**Bottleneck analysis** (from prior measurement):
- sim_per_output @ plotgap=20 ≈ **2.65 s**
- render_per_step @ B_pv64 (64 procs, 4 nodes) ≈ **0.61 s**
- **Sim is the bottleneck** → viz trigger strategy doesn't slow the run → fair comparison.

Estimated wall time per run: ~5 minutes (sim-bound).

## pvserver Configuration

Use the proven best config from prior eval:
- **64 procs across 4 nodes** (16 ppn): ares-comp-21, 22, 18, 19
- pvserver rank 0 on ares-comp-21 (local) so the bridge connects via `ares-comp-21:11112`

(Local-rendering Phase 1 alternative is deferred — using remote pvserver for now since
it's working and the comparison is about trigger strategy, not rendering mode.)

## Test Matrix

8 runs total. All use the same sim + pvserver + bridge setup. Only the consumer changes.

### Baseline (BL) — fixed-period visualization, no LLM

| ID | K (every N SST steps) | Max Vis Count |
|----|----------------------|---------------|
| BL-K1 | 1 | 100 |
| BL-K2 | 2 | 50 |
| BL-K5 | 5 | 20 |
| BL-K10 | 10 | 10 |

Each baseline run uses a Python script that calls MCP tools via stdio:
- Loop: `advance_step` → if `step % K == 0` → `create_isosurface(V, 0.3)` + `get_screenshot`
- Records timing + image bytes to `baseline_metrics.jsonl`
- Exits when `get_streaming_status` reports `ended: true`

### Agent (AG) — LLM-driven visualization

| ID | Model | Mode | Description |
|----|-------|------|-------------|
| AG-Haiku-Open | Haiku 4.5 | Open-ended | "Explore and screenshot interesting transitions" |
| AG-Haiku-Goal | Haiku 4.5 | Goal-directed | "Find when spot patterns first emerge" |
| AG-Sonnet-Open | Sonnet 4.5 | Open-ended | Same prompt as Haiku-Open |
| AG-Sonnet-Goal | Sonnet 4.5 | Goal-directed | Same prompt as Haiku-Goal |

Each agent run uses the existing `insitu_agent.py` (modified to track token usage)
with a per-variant prompt. The agent decides when to advance, when to screenshot,
and what visualizations to apply.

**API endpoint** (proxy):
```bash

```

**Rate limit awareness**: the proxy limits to 45 requests/min. The agent loop must
either rate-limit itself or accept some retries.

## Folder Structure

All artifacts go under `test/insitu_agent/results/bl_vs_ag/`:

```
results/bl_vs_ag/
├── PLAN.md                           # this file (copy)
├── COMPARISON.md                     # final analysis
├── comparison.csv                    # consolidated metrics
├── BL_K1/
│   ├── config.json
│   ├── streaming_timing.jsonl        # per-step (sst, pipe, render, total)
│   ├── baseline_metrics.jsonl        # per-decision (step, action, image_bytes)
│   ├── screenshots/                  # PNGs saved by the runner
│   ├── pvserver.log
│   ├── gray-scott.log
│   └── streaming_bridge.log
├── BL_K2/ ... BL_K10/                # same structure
├── AG_Haiku_Open/
│   ├── config.json
│   ├── streaming_timing.jsonl
│   ├── mcp_tool_timing.jsonl         # per-tool (pv, mcp, overhead)
│   ├── decisions.jsonl               # tool calls + reasoning
│   ├── token_usage.json              # input/output/image tokens, cost
│   ├── screenshots/                  # PNGs from get_screenshot
│   ├── agent_output.log              # full agent transcript
│   ├── pvserver.log
│   ├── gray-scott.log
│   └── streaming_bridge.log
└── AG_Haiku_Goal/ ... AG_Sonnet_Goal/
```

## Implementation Steps

### Step 1: Build the baseline runner

Create `run_baseline.py` (Python, uses MCP via stdio like `insitu_agent.py` but no LLM):

```python
# Pseudocode
async def main(K, results_dir):
    # Launch insitu_mcp_server.py as subprocess via stdio
    # Loop:
    #   call get_streaming_status
    #   if ended → break
    #   call advance_step
    #   if (step + 1) % K == 0:
    #     call create_isosurface(V, 0.3)  [first iteration only]
    #     call get_screenshot
    #     copy screenshot to results_dir/screenshots/step_N.png
    #     append to baseline_metrics.jsonl
```

Args: `--K`, `--results-dir`, `--server-host`, `--server-port`, `--pvpython`.

### Step 2: Add token tracking to `insitu_agent.py`

Modify `_call_anthropic()` to capture `response.usage`:
```python
total_input_tokens += response.usage.input_tokens
total_output_tokens += response.usage.output_tokens
# image input tokens are included in input_tokens for vision models
```

At end of run, write `token_usage.json`:
```json
{
  "model": "claude-haiku-4-5",
  "total_input_tokens": 75234,
  "total_output_tokens": 18420,
  "num_tool_calls": 87,
  "num_screenshots": 23,
  "estimated_cost_usd": 0.18,
  "wall_time_s": 312.5
}
```

Also save screenshots: hook into `get_screenshot` results and copy to `results_dir/screenshots/`.

### Step 3: Add prompt files

Create:
- `prompts/agent_open.txt` — open-ended exploration
- `prompts/agent_goal.txt` — goal-directed (find pattern emergence)

### Step 4: Build orchestration script

Create `run_bl_vs_ag.sh` that:
1. For each test variant: clean up, start pvserver, start sim, wait for SST, start bridge, run consumer, collect results
2. Saves into `results/bl_vs_ag/<VARIANT>/`
3. Tears down between runs

### Step 5: Build comparison analyzer

Create `analyze_bl_vs_ag.py`:
- Loads all `*/streaming_timing.jsonl`, `*/token_usage.json`, `*/baseline_metrics.jsonl`
- Computes per-variant metrics: vis count, wall time, total cost, image bytes, coverage
- Generates `COMPARISON.md` with side-by-side tables and a comparison.csv

## Metrics to Capture

| Metric | BL | AG | Source |
|--------|----|----|--------|
| `wall_time_s` | yes | yes | shell `time` |
| `num_visualizations` | yes | yes | count screenshots saved |
| `num_advance_calls` | yes | yes | count from log |
| `total_image_bytes` | yes | yes | sum of PNG file sizes |
| `coverage_ratio` | yes | yes | num_vis / 100 (output steps) |
| `decision_time_avg_ms` | yes (≈0) | yes | from logs |
| `input_tokens` | n/a | yes | Anthropic SDK `usage` |
| `output_tokens` | n/a | yes | Anthropic SDK `usage` |
| `total_cost_usd` | n/a | yes | computed from tokens × pricing |
| `streaming_render_avg_ms` | yes | yes | from `streaming_timing.jsonl` |
| `tool_pv_avg_ms` | n/a | yes | from `mcp_tool_timing.jsonl` |

## Token Cost Estimates

For 100-step runs (per the analysis in `AGENT_VS_BASELINE_PLAN.md`):

| Model | Per run |
|-------|---------|
| Haiku 4.5 | ~$0.20 |
| Sonnet 4.5 | ~$0.70 |

Total for 4 agent runs: ~**$1.80**.

## Execution Order

1. Implement `run_baseline.py` (~100 lines)
2. Add token tracking to `insitu_agent.py` (~30 lines)
3. Create prompt files
4. Run BL_K1 (validates pipeline + reproduces sim/pvserver setup)
5. Run BL_K2, BL_K5, BL_K10 sequentially
6. Run AG_Haiku_Open, AG_Haiku_Goal, AG_Sonnet_Open, AG_Sonnet_Goal sequentially
   - Insert ~30s rate-limit cooldown between consecutive AG runs
7. Run `analyze_bl_vs_ag.py` → generate `COMPARISON.md` + CSV

Estimated total wall time: **~50-60 minutes** (8 runs × ~6 min).
Estimated total token cost: **~$2**.

## Open Questions Before Starting

1. **Sim wall time** — at L=256, 128 procs, 2000 steps. We measured 264s for 64 procs;
   128 procs should be roughly 150-200s but may have communication overhead.
   First BL_K1 run will tell us.
2. **Agent rate limiting** — if proxy rate limit hits, we'll need to add explicit
   `await asyncio.sleep(1.5)` between calls in `run_agent_loop`.
3. **Goal-directed prompt content** — should we tell the agent the F/k parameters
   (which determine pattern type) or let it discover?

## Files to Create / Modify

| File | Action |
|------|--------|
| `run_baseline.py` | **NEW** — fixed-K MCP runner, no LLM |
| `insitu_agent.py` | **MODIFY** — add token usage capture + screenshot saving |
| `prompts/agent_open.txt` | **NEW** — open-ended exploration prompt |
| `prompts/agent_goal.txt` | **NEW** — goal-directed prompt |
| `run_bl_vs_ag.sh` | **NEW** — orchestration |
| `analyze_bl_vs_ag.py` | **NEW** — metric collection + report generation |
| `results/bl_vs_ag/` | **NEW** — output directory |
| `results/bl_vs_ag/COMPARISON.md` | **NEW** — final report |

---

**Ready to implement once you approve.**
