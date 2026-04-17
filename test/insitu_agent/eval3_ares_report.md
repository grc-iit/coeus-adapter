# Eval 3 — Ares: Gray-Scott `steps=5000 / plotgap=50` with 3-output LLM intercept

**Date:** 2026-04-07
**Cluster:** Ares (Skylake-AVX512, Ubuntu 22.04, OpenMPI 5.0.9 over TCP/eno1)
**Author:** hxu40
**Result directory:** `test/insitu_agent/results/gs_F008k003_inspect_20_22/AG_Haiku/`

---

## 1. Goal

Run a long Gray-Scott simulation in a regime that actually develops visible
patterns and demonstrate a **three-phase agent workflow**:

1. **Skip** outputs 1-19 with no visualization work,
2. **Inspect** outputs 20, 21, 22 with the LLM agent (advance + screenshot
   + describe — three "continuous LLM inferences"),
3. **Async drain** outputs 23-100 with no LLM and no rendering.

The goal is to measure (a) whether the in-situ pipeline can keep the bridge's
per-step bookkeeping out of the critical path on uninspected steps, and
(b) the wall-clock + token cost of the agent inspection itself.

## 2. Configuration

### 2.1 Gray-Scott simulation parameters

| Parameter | Value | Notes |
|---|---:|---|
| L (grid edge) | 256 | 256³ = 134 217 728 cells |
| Du / Dv | 0.2 / 0.1 | diffusion coefficients |
| F / k | **0.08 / 0.03** | aggressive regime, more dynamic than F=0.01/k=0.05 |
| dt | 1.0 | |
| plotgap | 50 | one SST output per 50 sim steps |
| steps | 5000 | total internal time steps |
| noise | 0.01 | initial perturbation amplitude (1000× higher than prior runs) |
| **Total SST outputs** | **100** | 5000 / 50 |
| **Total sim time** | 5000 units | 5000 × dt(1.0) |

Settings file: `settings-gs-F008k003-blocking.json`

### 2.2 ADIOS2 SST configuration (`adios2-sst-blocking.xml`)

```xml
<engine type="SST">
    <parameter key="RendezvousReaderCount" value="1"/>
    <parameter key="QueueLimit" value="5"/>
    <parameter key="QueueFullPolicy" value="Block"/>
    <parameter key="DataTransport" value="WAN"/>
    <parameter key="OpenTimeoutSecs" value="60.0"/>
</engine>
```

`Block` policy was chosen so that during the **inspection** phase, the
SST writer waits for the bridge to drain — i.e., the *sst_wait* time the
bridge records becomes a faithful upper bound on the LLM inference latency
that backs up into the writer.

### 2.3 Cluster topology

| Role | Procs | Nodes | ppn | Hosts |
|---|---:|---:|---:|---|
| Sim (`adios2-gray-scott`) | **128** | 8 | 16 | ares-comp-10..17 |
| pvserver (in-situ rendering) | **32** | 2 | 16 | ares-comp-21, 22 |
| Bridge + MCP server + agent | 1 | 1 | — | ares-comp-13 (login) |

The bridge connects to pvserver via `cs://ares-comp-21:11112`.

Process layout reported by gray-scott: `8x4x4` (each rank gets 32×64×64
local cells of the 256³ domain).

### 2.4 Software & versions

| Component | Version | Path / notes |
|---|---|---|
| OS | Ubuntu 22.04 | Linux kernel `5.15.0-171-generic` |
| Compiler | GCC 11.4.0 | (spack-managed) |
| OpenMPI | **5.0.9** | spack `openmpi-5.0.9-ruzpfypje...` |
| ADIOS2 | **2.11.0** | spack `adios2-2.11.0-gxiea6iqv...` |
| ParaView | **5.13.3** | spack `paraview-5.13.3-ssmv5hp4...` |
| Python (system) | 3.10.12 | `/usr/bin/python3` |
| Anthropic SDK | 0.89.0 | for `insitu_agent.py` |
| MCP Python SDK | (vendored) | `~/.local/lib/python3.10/site-packages/mcp/` |
| Gray-Scott reaction-diffusion | (custom build) | `/home/hxu40/software/gray-scott/build/adios2-gray-scott` (built 2026-04-04) |
| LLM provider | `claude-haiku-4-5-20251001` (Anthropic) via private proxy `https://yxai.anthropic.edu.pl` |

OpenMPI requires the following environment for cross-node TCP on Ares:

```bash
export OMPI_MCA_pml=ob1
export OMPI_MCA_btl=tcp,self
export OMPI_MCA_osc='^ucx'
export OMPI_MCA_btl_tcp_if_include=eno1
export OMPI_MCA_oob_tcp_if_include=eno1
```

Cross-node `mpirun` is launched through `~/software/gray-scott/ssh-spack-wrapper.sh`
which injects the spack environment into ssh-spawned shells, so `prted` and
`pvserver` are findable on remote nodes.

### 2.5 The three-phase agent prompt

`prompts/agent_inspect_20_22.txt` instructs Haiku to:

```
PHASE 1 (SKIP) — outputs 1..19
  19 × advance_step, no screenshots, one-word reasons.
PHASE 2 (INSPECT) — outputs 20, 21, 22
  For each: advance_step → get_screenshot → 1-sentence description.
PHASE 3 (ASYNC DRAIN) — outputs 23..100
  Single resume_streaming call, then a closing sentence, then STOP.
Setup before Phase 1: get_streaming_status + create_isosurface(V, 0.3)
```

### 2.6 Bridge optimisation: `--render-steps`

A new CLI arg on `insitu_streaming.py` filters which SST outputs are
actually rendered + saved to disk. The runner passes:

```
--render-steps 20,21,22
```

On every other step the bridge calls `PrepareNextStep` and
`UpdatePipelineInformation` (mandatory for SST consumption), increments
its step counter, writes a timing record, **and skips**
`fides.UpdatePipeline` + `Render(view)` + `SaveScreenshot`.

This is the key optimisation that decouples the bridge from the bulk of
the simulation — Phase 1 and Phase 3 incur ~18 ms of bridge work per
step instead of ~1.6 s.

## 3. How to reproduce

```bash
cd /mnt/common/hxu40/coeus/iowarp/coeus-adapter/test/insitu_agent

# Verify cluster nodes are in your SLURM allocation:
sinfo -N -h -o '%N %t' | grep -E 'ares-comp-(1[0-9]|2[0-2])'

# Spack envs (loaded inside the runner script):
spack load adios2@2.11.0/g
spack load paraview

# Set the LLM endpoint:
export ANTHROPIC_BASE_URL="https://yxai.anthropic.edu.pl"
export ANTHROPIC_API_KEY="sk-…"

# Launch (this is the entire experiment in one script):
bash run_gs_F008k003_inspect_20_22.sh

# Output:
#   results/gs_F008k003_inspect_20_22/AG_Haiku/
#     ├── pvserver.log
#     ├── gray-scott.log
#     ├── streaming_bridge.log         (per-step bridge timing prints)
#     ├── streaming_timing.jsonl       (one JSON record per bridge step)
#     ├── mcp_tool_timing.jsonl        (one JSON record per MCP tool call)
#     ├── consumer.log                 (full agent transcript + tool calls)
#     ├── token_usage.json             (Haiku usage + cost summary)
#     ├── config.json                  (variant metadata)
#     └── screenshots/
#         ├── agent_0001.png   # output 20
#         ├── agent_0002.png   # output 21
#         └── agent_0003.png   # output 22
```

The runner orchestrates:
1. start `pvserver` on 32 procs / 2 nodes,
2. launch `adios2-gray-scott` on 128 procs / 8 nodes,
3. start `insitu_streaming.py` (the bridge) once the SST contact file appears,
4. run `insitu_agent.py` with the 3-phase prompt under a 720 s shell timeout
   and a 900 s in-process wall cap,
5. send `{"action":"resume"}` to drain the rest, then wait for the sim PID,
6. write `config.json` with `wall_time_s` and exit.

## 4. Results

### 4.1 Top-line numbers

| Metric | Value |
|---|---:|
| Total wall time | **999.9 s** (~16.7 min) |
| Sim wall time (rank ET) | **977.0 s** |
| Per-output sim cost (averaged) | **9.77 s/output** |
| Per internal sim step | 0.195 s/step |
| Bridge step records written | 110 (10 stale tail past sim end) |
| Sim outputs produced | **100 / 100 ✓** |
| Screenshots saved | **3 / 3 ✓** (distinct MD5s) |
| Agent loop wall time | 192.4 s |
| Phase 2 (3 intercepts) wall time | **36.27 s** |
| Per intercept (avg) | 12.1 s |
| Token cost (run total) | **\$0.01696** |
| Phase 2 share of token cost | ~\$0.00526 (31 %) |
| Cost per intercept | ~\$0.00175 |

### 4.2 Per-step bridge timing (RENDER vs SKIP)

The `--render-steps 20,21,22` flag splits the bridge's per-step work into
two regimes:

| Step type | n | sst_wait | pipeline | render | step_total |
|---|---:|---:|---:|---:|---:|
| **RENDER** (steps 20, 21, 22) | 3 | 6.3 ms | 1197 ms | 1120 ms | **2324 ms** |
| **SKIP** (every other step) | 107 | 3.9 ms | 0 ms | 0 ms | **18 ms** |

→ **SKIP steps are ~130× cheaper than RENDER steps.** The 107 non-inspection
bridge iterations contribute only ~1.9 s of bookkeeping in total.

### 4.3 Bridge phase boundaries (relative to bridge start)

```
Phase 1 (skip 1-19):    0.0 s →  104.4 s   (104.4 s for 19 steps,  ~5.5 s/step)
                                            limited by Haiku LLM 1 call/step
inter-phase gap:      104.4 s →  115.6 s   (~11 s, agent decides to start Phase 2)
Phase 2 (inspect 20-22): 115.6 s →  144.2 s (28.6 s for 3 intercepts, ~9.5 s each)
inter-phase gap:      144.2 s →  155.2 s   (~11 s, resume_streaming + agent exit)
Phase 3 (drain 23-100): 155.2 s →  309.7 s (154.5 s for 78 drain steps)
Stale tail (>100):    309.7 s →  329.8 s   (10 over-shoots until --max-steps cap)
```

After bridge stops at ~330 s, the shell waits for sim to complete its
remaining internal steps (sim was running ahead of the bridge thanks to
the 5-deep SST queue). Sim completes at the `wall_time_s = 999.9 s` mark.

### 4.4 Decomposition of one intercept (output 21, 12.66 s)

```
T=0.00 s ─┬─ get_screenshot for output 20 returned image to LLM
PHASE A   │   LLM digests image 20, writes 1-sentence description,
4.53 s    │   plans next advance_step, round-trips through proxy
T=4.53 s ─┼─ MCP advance_step writes "advance_one" command file
PHASE B   │   Bridge picks up command (~500 ms poll latency),
2.66 s    │     SST wait      :     6 ms  (queue had data ready)
          │     UpdatePipeline: 1 194 ms  ← dominant bridge cost
          │     Render+Save   :   400 ms
T=7.20 s ─┼─ Bridge step 21 timing record written
PHASE C   │   Agent gets streaming_status, decides to screenshot
5.47 s    │   (another LLM round-trip), MCP get_screenshot reads
          │   bridge_latest.png (~1 ms), bytes returned to LLM
T=12.66 s ─── Intercept 21 complete
```

| Phase | Time | Share |
|---|---:|---:|
| A. LLM "think + plan" round-trip | 4.53 s | 36 % |
| B. advance_step → bridge done | 2.66 s | 21 % |
| &nbsp;&nbsp;&nbsp;&nbsp;sst_wait | 6 ms | — |
| &nbsp;&nbsp;&nbsp;&nbsp;Fides UpdatePipeline | 1 194 ms | — |
| &nbsp;&nbsp;&nbsp;&nbsp;Render + SaveScreenshot | ~400 ms | — |
| C. bridge done → screenshot in agent | 5.47 s | 43 % |

Output 22 has nearly the same shape (12.17 s; A=6.73, B=2.89, C=2.55 s).
Output 20 is slightly heavier (11.43 s of *wall* but with **2.52 s render**
because of first-frame VTK volume mapper shader compilation).

### 4.5 Token consumption

| Token type | Count | Rate (\$/M) | Cost |
|---|---:|---:|---:|
| Input (uncached) | 513 | \$1.00 | \$0.00051 |
| Cache creation | 4 669 | \$1.25 | \$0.00584 |
| Cache read | 18 626 | \$0.10 | \$0.00186 |
| Output | 1 756 | \$5.00 | \$0.00878 |
| **TOTAL** | — | — | **\$0.01696** |

- 29 LLM calls, 28 MCP tool calls
- Output dominates cost (~52 %), then cache_creation (~34 %)
- Per intercept (Phase 2 share): **~\$0.00175**
- The full Anthropic prompt-cache flywheel is doing real work: 18 626
  tokens read from cache at \$0.10/M instead of \$1.00/M (≈\$0.017 saved
  on this single run alone)

### 4.6 Screenshot integrity

Three distinct MD5 hashes, growing PNG sizes (more pattern complexity →
worse compression as time progresses):

```
3177fb127190526ef013cb9d0b082140  agent_0001.png  10 739 B  output 20  sim step 1000
148618f5aec00b7725c044d125c83cea  agent_0002.png  11 605 B  output 21  sim step 1050
6d57d5903411172ba52fc591cf6fb7eb  agent_0003.png  12 422 B  output 22  sim step 1100
```

Each PNG is a 1024×768 volume rendering of `V` colored by the auto-rescaled
transfer function. The data shows a centered, growing reactive blob — the
expected early-time Gray-Scott pattern that hasn't yet spread to fill the
domain.

### 4.7 MCP tool call summary

| Tool | Count |
|---|---:|
| `get_streaming_status` | 1 |
| `create_isosurface` | 1 |
| `advance_step` | **22** (19 in Phase 1 + 3 in Phase 2) |
| `get_screenshot` | **3** |
| `resume_streaming` | **1** (Phase 3 trigger) |
| **Total** | **28** |

→ Exactly the call sequence the prompt asked for.

## 5. Observations

1. **The render-steps optimisation completely removes the bridge from the
   critical path on uninspected outputs.** Per-step bridge bookkeeping on
   SKIP steps is ~18 ms, vs ~2 300 ms when rendering. Across 107 SKIP
   steps that's a savings of ~245 s — without it the bridge would back-pressure
   the sim's SST queue, slowing the sim's effective rate.

2. **The wall time is sim-compute-bound, not bridge or LLM bound.**
   - Sim wall: 977 s
   - Total wall: 999.9 s
   - The bridge consumes its 100 outputs in ~310 s of bridge wall while
     sim is running ahead, then the shell waits ~670 s for sim to complete
     its remaining internal time stepping.

3. **The "13 s/output" we measured in the prior render-all run was an
   artefact of bridge back-pressure**, not the true sim rate. Once the
   bridge stops doing per-step rendering, sim's actual native rate is
   ~9.77 s/output.

4. **Each LLM intercept is ~12 s wall, of which ~83 % is LLM round-trip
   time (Phases A + C) and only ~17 % is actual bridge work + sim wait.**
   Two separate LLM round-trips per intercept (one to advance, one to
   screenshot) means each intercept incurs the proxy round-trip latency
   twice. Collapsing them into a hypothetical single tool call would
   save ~3-5 s per intercept.

5. **`Fides UpdatePipeline` (1.19 s) is the dominant *bridge* cost on
   render steps**, not the volume rendering itself (~400 ms). This is
   the time to deserialise the V field from the SST stream into VTK
   memory. Render is comparatively cheap.

6. **First-frame penalty: step 20 had a 2.52 s render vs ~0.40 s on
   steps 21/22.** That's VTK compiling the volume mapper shaders the
   first time the volume representation is rendered.

7. **Bridge over-shoots after sim ends** (10 stale steps past output
   100 until `--max-steps 110` fired). This is the same Fides quirk we
   saw in earlier runs — `PrepareNextStep` returns READY for stale data
   when the writer has finished but hasn't cleanly signalled
   END_OF_STREAM. The hard `--max-steps` cap is the right defence.

8. **Token cost is essentially flat in screenshot count.** Going from
   3 to 8 to N screenshots adds O(N) image tokens (~1 437 each), but
   the per-turn conversation context (read from cache at \$0.10/M)
   dominates everything. 100 outputs × 3 inspections cost \$0.017,
   well under one cent per intercept.

9. **`Block` SST policy with `QueueLimit=5` is sufficient** for this
   workload. The queue never overflowed (sim was producing only ~5 %
   faster than the bridge consumed during Phase 3), so we never hit
   the discard scenario.

## 6. Bugs uncovered and fixed during this experiment series

In chronological order:

1. **Empty text block in `_call_anthropic`** —
   `anthropic.BadRequestError: messages: text content blocks must be non-empty`
   when Haiku produced a tool-use turn with no preceding text. The proxy
   used to filter empty blocks silently; the direct API enforces the spec.
   Fix: only emit a text block in the assistant message when non-empty;
   pad to a single space when there are no tool uses either.

2. **Bridge stale-data heuristic false-positives** — an early heuristic
   that killed the bridge when render times stayed below 200 ms for 4
   consecutive steps. Triggered every time the bridge view didn't have a
   heavy filter visible. Removed; replaced with the simpler `--max-steps`
   hard cap.

3. **Bridge `SaveScreenshot` extension bug** — `vtkSMSaveScreenshotProxy`
   infers format from extension and rejected `bridge_latest.png.tmp`.
   Fix: write to `bridge_latest.tmp.png` (extension at the end), then
   atomic `os.replace`.

4. **MCP `get_screenshot` reading from a separate ParaView client state** —
   the bridge process and the MCP server are *separate* `pvpython`
   processes, each with its own `paraview.simple` local view state. Even
   though both connect to the same pvserver, MCP's `Render` + `SaveScreenshot`
   operated on its own near-empty view, producing the 2 170-byte placeholder
   PNG every time. Fix: the bridge atomically writes its own rendered view
   to `bridge_latest.png`, and `insitu_mcp_server.py:get_screenshot` now
   reads that file first (with a fallback to `pv_manager.get_screenshot`).

5. **Bridge view showed only the grid wireframe instead of the V field** —
   `setup_initial_display` originally used `Show(fides, view, "UniformGridRepresentation")`
   which defaulted to Outline. Replaced with `Volume` representation
   colored by V (with Outline fallback if Volume isn't supported on this
   data). Combined with `fides.UpdatePipeline()` per step to force fresh
   reads, this is what makes the agent's screenshots actually show data.

6. **Per-step bridge work was being done even on uninspected outputs** —
   added `--render-steps` filter so the bridge skips
   `UpdatePipeline + Render + SaveScreenshot` on steps the agent will
   never look at. **This is the change that this report is about.**

## 7. Conclusions

- For a simulation that produces 100 outputs over ~16 minutes of compute,
  a Haiku agent can intercept 3 specific outputs, generate per-output
  descriptions, and resume async drain — at a total cost of **\$0.017
  and ~36 seconds of agent wall time** (out of 1000 s total run wall).

- The **`--render-steps` filter is the right architectural primitive**:
  it lets the bridge stay decoupled from the sim's main execution while
  still being available to render any specific frame the agent asks for.
  Without it, the bridge would render all 100 outputs and add 100 × 1.6 s
  = 160 s of unnecessary work that would back-pressure the SST writer.

- **The dominant cost of an LLM intercept at this configuration is
  the LLM proxy round-trip, not anything inside the in-situ pipeline.**
  Each intercept spends ~10 s in LLM/proxy network latency vs ~2 s in
  the bridge and ~0 s in MCP overhead. A pipeline change can't shave
  that 10 s; only collapsing the two-call (advance + screenshot)
  pattern into a single MCP tool can.

- **`Fides UpdatePipeline` is the bridge's hot path on render steps**
  (~1.2 s out of ~1.6 s of bridge work). If we needed to render
  *every* output, optimising the SST → VTK deserialisation path would
  be the next thing to look at — but for the inspect-only workflow we
  use here, it doesn't matter because we only call it 3 times.

- **All four real bugs surfaced by this experiment series have been
  fixed in `insitu_streaming.py`, `insitu_mcp_server.py`, and
  `insitu_agent.py`** and are now part of the in-tree pipeline.

## 8. Files of record

| Path | Purpose |
|---|---|
| `test/insitu_agent/settings-gs-F008k003-blocking.json` | Gray-Scott params + adios config pointer |
| `test/insitu_agent/adios2-sst-blocking.xml` | SST `Block` policy, queue limit 5 |
| `test/insitu_agent/prompts/agent_inspect_20_22.txt` | The three-phase prompt |
| `test/insitu_agent/run_gs_F008k003_inspect_20_22.sh` | Single-variant runner |
| `test/insitu_agent/insitu_streaming.py` | Bridge with `--render-steps` and `--screenshot-file` |
| `test/insitu_agent/insitu_mcp_server.py` | MCP server with bridge-saved-screenshot path |
| `test/insitu_agent/insitu_agent.py` | Haiku driver loop with token usage capture |
| `test/insitu_agent/results/gs_F008k003_inspect_20_22/AG_Haiku/` | All raw artifacts (logs, jsonl, screenshots) |
| `test/insitu_agent/results/gs_F008k003_inspect_20_22/AG_Haiku_render_all/` | Earlier render-on-every-step run for comparison |
