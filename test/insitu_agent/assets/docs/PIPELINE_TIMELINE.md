# Trigger-Render-Reason Pipeline: Timeline Analysis (record)

**Date:** 2026-07-20 · **Mode:** asynchronous (non-blocking) render-reason
**Run:** 128 producer ranks → 16-rank pvserver consumer + AI agent
**Artifacts:** `results/agent_128p_timeline_0720/`
(`trigger_timeline.jsonl`, `streaming_timing.jsonl`, `frames/frames.jsonl`,
`mcp_tool_timing.jsonl`, `token_usage.json`, `TIMELINE_ANALYSIS.txt`)

Regenerate with (from `test/insitu_agent/`): `python3 assets/scripts/analyze_timeline.py results/agent_128p_timeline_0720`

---

## 1. Configuration

| | |
| --- | --- |
| Producer | 128 ranks @ ppn=32 on `ares-comp-28/29/30/31`, `iowarp@dev` + `adios2-coeus@vigil` |
| Domain / regime | `L=256`, F=0.08, k=0.03, dt=1, plotgap=50, steps=1000 → **20 output steps** |
| Engine / trigger | `hermes_derived`, `variance(derive/VarV)`, gated SST, `baseline_ratio=10`, `inspect_steps=4` |
| Consumer | 16-rank `pvserver` + greedy bridge (`--frames-dir`, no `--paused`) on `ares-comp-27` |
| Agent | `claude-haiku-4-5`, async prompt (`prompts/agent_async_verdict_256.txt`), disk-frame verdict |

## 2. Instrumentation (single wall clock)

All sources emit **system epoch seconds** (engine `std::chrono::system_clock`
== Python `time.time()`), so producer, bridge, and agent events align directly
(NTP-synced nodes).

| Source | File | Events |
| --- | --- | --- |
| Engine (rank 0) | `trigger_timeline.jsonl` | `sim_start`, `step_begin`, `output`, `fire`, `ship` - via `HermesEngine::LogTimeline_` (`src/hermes_engine.cc`) |
| Bridge | `streaming_timing.jsonl` · `frames/frames.jsonl` | per flagged step: `sst_wait`, `pipeline_update` (pull), `render`; per frame: wall + V range |
| MCP server | `mcp_tool_timing.jsonl` | per tool call wall + duration |
| Agent | `token_usage.json` | `start_epoch`, `end_epoch` (verdict), tokens, cost |

`T = 0` is `sim_start` (the writer begins stepping, after SST rendezvous).

## 3. Producer per-step timeline (all 20 outputs)

- **begin** = `BeginStep` (compute starts) · **produced** = field + derived vars in CTE
- **compute** = begin→produced · **EndStep** = produced→next begin (finalize; for
  flagged steps includes the SST ship + `Block` wait) · **cadence** = begin→next begin

| step | begin | produced | compute | EndStep | cadence | ship | note |
|---:|---:|---:|---:|---:|---:|---:|---|
| 1 | T+0.0 | T+1.7 | 1.73 s | 1.87 s | 3.60 s | – | |
| 2 | T+3.6 | T+5.1 | 1.49 s | 2.08 s | 3.57 s | – | |
| 3 | T+7.2 | T+8.4 | 1.24 s | 2.51 s | 3.75 s | – | |
| 4 | T+10.9 | T+12.5 | 1.55 s | 2.30 s | 3.85 s | – | |
| 5 | T+14.8 | T+16.5 | 1.77 s | 2.16 s | 3.93 s | – | |
| 6 | T+18.7 | T+19.9 | 1.24 s | 2.55 s | 3.78 s | – | |
| 7 | T+22.5 | T+24.2 | 1.68 s | 1.99 s | 3.67 s | – | |
| **8** | T+26.2 | T+27.7 | 1.57 s | 3.24 s | **4.81 s** | T+29.1 | **FIRE** var=0.000501 |
| 9 | T+31.0 | T+31.9 | 0.90 s | 3.87 s | 4.76 s | T+33.9 | window (ship Blocks) |
| 10 | T+35.7 | T+36.4 | 0.66 s | 5.05 s | 5.71 s | T+39.7 | window (ship Blocks) |
| 11 | T+41.4 | T+45.7 | 4.27 s | 2.57 s | 6.85 s | T+46.6 | window (ship Blocks) |
| 12 | T+48.3 | T+50.4 | 2.14 s | 2.43 s | 4.57 s | – | drain tail |
| 13 | T+52.8 | T+53.5 | 0.66 s | 2.94 s | 3.60 s | – | |
| 14 | T+56.4 | T+57.1 | 0.68 s | 2.84 s | 3.53 s | – | |
| 15 | T+60.0 | T+60.9 | 0.95 s | 2.78 s | 3.73 s | – | |
| 16 | T+63.7 | T+65.2 | 1.47 s | 2.12 s | 3.59 s | – | |
| 17 | T+67.3 | T+68.9 | 1.59 s | 1.99 s | 3.58 s | – | |
| 18 | T+70.9 | T+72.5 | 1.67 s | 1.94 s | 3.60 s | – | |
| 19 | T+74.5 | T+76.0 | 1.47 s | 1.95 s | 3.42 s | – | |
| 20 | T+77.9 | T+79.2 | 1.34 s | – | – | – | last output |

**Summary:** free-run cadence **3.75 s/output** (compute ~1.5 s + EndStep/CTE
~2.2 s); window steps 8–11 stretch to **4.8 / 4.8 / 5.7 / 6.85 s** under `Block`
back-pressure; cadence returns to ~3.5 s after the window. Total
`sim_start → output 20` = **79.2 s**.

> The compute/EndStep split smears across the window boundary (step 10 shows tiny
> compute but 5.05 s EndStep = it absorbed the ship-Block; step 11's 4.27 s
> "compute" is its begin delayed by step 10's Block). **cadence** is the clean
> per-step metric.

## 4. Flagged window (steps 8–11) → AI verdict

### Per flagged step: producer → bridge → disk

| flagged | produced | ship (SST) | frame on disk | bridge pull | render |
|---|---:|---:|---:|---:|---:|
| step 8 | T+27.7 | T+29.1 | T+35.4 | 2.75 s | 0.72 s |
| step 9 | T+31.9 | T+33.9 | T+41.4 | 2.38 s | 0.73 s |
| step 10 | T+36.4 | T+39.7 | T+47.1 | 2.39 s | 0.68 s |
| step 11 | T+45.7 | T+46.6 | T+52.8 | 2.40 s | 0.68 s |

Frame cadence (frame 1 → frame 4): **17.4 s over 3 gaps = 5.8 s/frame**.

### Fire → verdict decomposition (total 30.3 s)

| segment | time | share |
|---|---:|---:|
| fire → 1st flagged frame on disk | 6.9 s | 22.9% |
| frame 1 → frame 4 (bridge drains other 3) | **17.4 s** | **57.4%** |
| frame 4 on disk → `get_flagged_frames` returns (disk read+encode) | 0.3 s | 1.1% |
| `get_flagged_frames` → **VERDICT** (LLM analyzes 4 images) | 5.6 s | 18.5% |

### Where the window time goes (per frame ~5.8 s)

| component | time |
| --- | ---: |
| bridge Fides pull (SST → VTK, 256³) | **2.48 s** |
| bridge volume render → PNG | 0.70 s |
| bridge active work / frame | 3.18 s |
| waiting on next producer ship (`QueueLimit=1` Block) | ~2.61 s |

- The AI's **own** cost is only the **5.6 s** LLM analysis. `get_flagged_frames`
  reports "blocked 29.1 s," but that is pure waiting that **overlaps** the window
  drain (the agent started at T+16.6, before the fire); it adds **0 s** to the
  wall clock.
- ~80% of fire→verdict is **getting the 4 frames rendered to disk**, dominated by
  the **2.48 s/frame Fides pull** and the **~2.6 s/frame producer-ship wait**.

## 5. Decoupling (async vs sync)

| | Synchronous (`--paused` + `advance_step`) | **Asynchronous** (`--frames-dir`) |
| --- | --- | --- |
| Added producer stall during window | **~65 s** (LLM-paced) | **~7 s** (steps 8–11 at 5.1 vs 3.75 s/output) |
| Agent on critical path? | yes | **no** - reads frames from disk |
| Agent verdict | 68–97 s, ~10 tool calls | **42 s, 1 tool call, $0.017** |
| Producer completion | gated on agent | **output 20 at T+79.2 s while agent verdicted at T+58.7 s** |

**Proof of decoupling:** the agent issued its verdict at **T+58.7 s**, and the
simulation ran on to complete all 20 outputs at **T+79.2 s**. The agent finished
**20.5 s before** the simulation did.

## 6. Optimization levers (verdict latency)

The bottleneck is the **17.4 s window drain**, not the model:
1. **Verdict on fewer frames**: `get_flagged_frames(min_frames=1–2)`; the agent
   can usually judge "healthy pattern forming" from frame 1 → saves ~12 s.
2. **`QueueLimit = inspect_steps`** (one-line engine change): the producer dumps
   all 4 flagged steps into the SST buffer at once, removing the ~2.6 s/frame
   ship wait; drain shrinks toward the bridge's raw ~3.2 s/frame × 4 ≈ 13 s and
   the producer-side stall drops too.
3. Faster Fides pull (2.48 s/frame): the largest single per-frame cost.

The LLM analysis itself (5.6 s) is already the cheap part.
