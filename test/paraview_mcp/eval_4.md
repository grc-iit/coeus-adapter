# eval_4 — 30-case ParaView MCP needle-in-a-haystack benchmark

**Date:** 2026-04-08
**Benchmark:** 30 synthetic VTK volumes with planted anomalies (10 hotspot + 10 cavity + 10 blobs)
**Agent:** `claude-sonnet-4-6` driving `paraview_mcp_server.py` over stdio
**Headline:** **27 / 30 hits (90%), $1.38 total, 234 tool calls across 261 turns**

---

## 1. Goal

Demonstrate that an LLM agent can use the ParaView MCP "properly" — and quantify
*how* properly. Approach: generate synthetic 3-D volumes with planted needles
(known ground truth), give the agent a natural-language task that can only be
answered by tool use against the MCP, score whether the model finds the needle.

The benchmark is designed to do double duty:

1. Measure agent capability on real visualization-style tasks.
2. Surface latent defects in the MCP server itself — the act of running it
   should expose anything that was broken or missing.

---

## 2. Environment

| Component | Version |
|---|---|
| OS | Ubuntu 22.04.5 LTS (Linux 5.15.0-171-generic, x86_64) |
| ParaView | 5.13.3 (spack build, skylake_avx512) |
| pvpython / Python | 3.12.12 |
| VTK | 9.3.20240617 |
| numpy | 2.4.4 |
| anthropic SDK | 0.86.0 |
| mcp Python SDK | 1.26.0 |
| Model | `claude-sonnet-4-6` |
| API endpoint | `https://yxai.anthropic.edu.pl` (Vertex AI front, owned_by `vertex-ai`) |

`pvserver` ran in a restart loop on `localhost:11111` so each per-case MCP
spawn could connect cleanly:

```bash
while true; do
  $PVDIR/pvserver --multi-clients --server-port=11111 >> ~/paraview_logs/pvserver.log 2>&1
  sleep 0.3
done
```

ParaView's `pvserver --multi-clients` exits when its last client disconnects in
this version, so the loop is required to keep the port live across cases.

---

## 3. What was run

### 3.1 Synthetic data generator

`benchmark/gen_needles.py` produces 64×64×64 VTK ImageData volumes plus a
sidecar `*.truth.json` per case (private to the scorer, never shown to the
agent), and a single `manifest.json` with the natural-language task per case.

Three needle types:

| type | scalar field | task |
|---|---|---|
| **hotspot** | `Temperature` flat background ~1.0 with one Gaussian peak | "Find the (x,y,z) voxel of the peak" |
| **cavity** | `Density` solid sphere with a smaller off-center hollow inside | "Locate the cavity center and report its radius" |
| **blobs** | `Intensity` 2–5 disconnected Gaussian blobs at random positions | "How many distinct objects? Where are they?" |

Generation command (already-executed):

```bash
pvpython benchmark/gen_needles.py --per-type 10 --out benchmark/cases_30
```

→ 30 `.vti` + 30 `.truth.json` + 1 `manifest.json` in `benchmark/cases_30/`.

### 3.2 Runner

`benchmark/run_benchmark.py` is a tool-use loop:

1. Spawns `paraview_mcp_server.py` as a stdio subprocess (each case gets a
   fresh MCP server, which connects to the running pvserver).
2. Calls `session.list_tools()` and converts to Anthropic-style tool schemas.
3. Loops `client.messages.create()` ↔ `session.call_tool(...)` until the
   model produces an `end_turn` text response or `--max-turns` is reached.
4. Forwards image content from MCP results as Anthropic image blocks (so
   vision-capable models can actually see screenshots).
5. Captures full transcript per case: every assistant message, every tool call,
   every tool result, every per-turn `usage` (`input_tokens`, `output_tokens`,
   `cache_read_input_tokens`, `cache_creation_input_tokens`).
6. On transient proxy errors (502/503/504, timeouts, connection errors,
   422 with markers like `"no available account"`, `"upstream"`,
   `"unavailable"`, `"temporarily"`), retries up to 5× with exponential
   backoff (5s, 10s, 20s, 40s, 80s).

Run command for this evaluation:

```bash
ANTHROPIC_BASE_URL=https://yxai.anthropic.edu.pl \
ANTHROPIC_AUTH_TOKEN=<token> \
pvpython benchmark/run_benchmark.py \
  --manifest benchmark/cases_30/manifest.json \
  --out      benchmark/transcripts/sonnet_30 \
  --model    claude-sonnet-4-6 \
  --max-turns 20 \
  --verbose
```

### 3.3 Scorer

`benchmark/summarize.py` reads each transcript + sidecar truth file and
extracts the model's predicted answer with regex parsers per needle type:

| needle | extract | hit criterion |
|---|---|---|
| hotspot | `(x,y,z)` integer triple | distance ≤ 2 voxels (exact = "hit", ≤5 = "near") |
| cavity | `(x,y,z)` and radius | center distance ≤ 3 AND radius within 2 |
| blobs | integer count of objects | exact count match |

The scorer supports many natural phrasings (`r ≈ 7 voxels`, `Total distinct
blobs: 3`, `**(x, y, z) = (9, 47, 39)**`, etc.). Two false negatives in the
first scoring pass were traced to parser misses, not model errors, and the
parser was tightened.

---

## 4. Bugs surfaced and fixed during the benchmark work

The benchmark surfaced 9 latent defects in the ParaView MCP. All were fixed
before the 30-case sweep. Each was a real blocker, not cosmetic.

| # | File | Bug | Fix |
|---|---|---|---|
| 1 | `paraview_mcp_server.py:26` | Existing stdio workaround patched `sys.stdout`/`sys.stderr` but **not** `sys.stdin`. `mcp.server.stdio.stdio_server` reads `sys.stdin.buffer` under pvpython, where `sys.stdin` is a `vtkPythonStdStreamCaptureHelper` that lacks `.buffer`. The MCP server has been crashing at startup since March (visible in `~/paraview_logs/paraview_mcp_external.log`). | Patch all three streams to real fds wrapped in `TextIOWrapper`. |
| 2 | `paraview_manager.py:122` | `load_data` called `view.ResetCamera(False)` where `view = GetActiveView()`. With a headless pvserver and no GUI, `GetActiveView()` returns `None` until `Show()` runs, so every load returned an error string even though data did load. | Re-fetch `GetActiveView()` after `Show()` and null-check. |
| 3 | `paraview_manager.py:1175` | `get_screenshot` did `sys.exit(1)` if no render view found — would kill the MCP server mid-benchmark. | Return `(False, error_msg, None)` instead. |
| 4 | `paraview_manager.py:741` | `get_histogram` was non-idempotent. After the first call, the `Histogram1` filter became the active source; subsequent calls tried to histogram a `vtkTable` and crashed with `vtkPExtractHistogram: Attempt to get point or cell data from a data object`. | Walk back via `self.original_source`; reuse a cached Histogram filter so the pipeline stays clean. |
| 5 | `paraview_manager.py:1238` | `plot_over_line` was write-only — created the filter but never returned the sampled values. The model had no way to read the data it had asked for. | Fetch the resulting `vtkPolyData`, return per-sample `(t, value, x, y, z)` rows from real polyline coordinates. |
| 6 | `paraview_mcp_server.py` | `get_histogram` existed in the manager but wasn't exposed as an MCP tool. | Added as an MCP tool. |
| 7 | `paraview_mcp_server.py` | `plot_over_line` MCP tool returned a useless one-line confirmation. | Server-side wrapper now formats the sample list with a header reporting the argmax + xyz, followed by tab-separated rows. |
| 8 | `paraview_manager.py` get_screenshot | Tiny PNG (no viewport size set) → vision-capable models couldn't read it. | Force `gui_view.ViewSize = [1024, 768]` and pass `ImageResolution=[1024, 768]` to `SaveScreenshot`. |
| 9 | `paraview_manager.py` + `paraview_mcp_server.py` | **Missing tool**: no way to enumerate connected components. Cases 4 and 5 of an earlier 6-case sweep both ran 25 turns without converging because the model could only randomly probe lines looking for blob peaks. | Added `find_connected_components(threshold, field)` wrapping `Threshold + Connectivity` filters; returns per-region `{centroid, bounds, n_points}`. |

Bug 9 is the structural finding: the benchmark **predicted** the missing tool by
showing every model failing the same way on the same case category. After
adding it, both cases converged in 4–5 calls.

Two fixes in the runner itself were also needed:

- The runner had to apply the same stdin/stdout/stderr restoration trick under
  pvpython (the mcp client side also reads them).
- Tool result image content was originally being dropped before reaching the
  model — fixed to forward as Anthropic image blocks.

---

## 5. Results

### 5.1 Headline

```
30 cases, 27 hits (90%), 261 turns, 234 tool calls
input=4932 tokens   output=44339 tokens
cache_read=684831 tokens   cache_create=165049 tokens
cost ≈ $1.3805
```

### 5.2 Per category

| category | hits | rate | avg tool calls | avg cost / case |
|---|---:|---:|---:|---:|
| **hotspot** (10) | **10 / 10** | **100%** | 5.0 | $0.017 |
| **cavity** (10) | **7 / 10** | **70%** | 14.4 | $0.108 |
| **blobs** (10) | **10 / 10** | **100%** | 4.2 | $0.015 |
| **TOTAL** (30) | **27 / 30** | **90%** | 7.8 | **$0.046** |

### 5.3 Per case

```
case  needle    stop                  turns calls  cost$   score    detail
   0  hotspot   end_turn                 6     5  0.0190   hit      pred=[9, 47, 39]   truth=[9, 47, 39]   d=0
   1  hotspot   end_turn                 6     5  0.0150   hit      pred=[48, 22, 24]  truth=[48, 22, 24]  d=0
   2  hotspot   end_turn                 6     5  0.0208   hit      pred=[13, 15, 22]  truth=[13, 15, 22]  d=0
   3  hotspot   end_turn                 6     5  0.0140   hit      pred=[38, 27, 14]  truth=[38, 27, 14]  d=0
   4  hotspot   end_turn                 6     5  0.0177   hit      pred=[22, 32, 32]  truth=[22, 32, 32]  d=0
   5  hotspot   end_turn                 6     5  0.0262   hit      pred=[43, 44, 26]  truth=[43, 44, 26]  d=0
   6  hotspot   end_turn                 6     5  0.0132   hit      pred=[43, 41, 10]  truth=[43, 41, 10]  d=0
   7  hotspot   end_turn                 6     5  0.0177   hit      pred=[30, 11, 45]  truth=[30, 11, 45]  d=0
   8  hotspot   end_turn                 6     5  0.0155   hit      pred=[9, 42, 24]   truth=[9, 42, 24]   d=0
   9  hotspot   end_turn                 6     5  0.0131   hit      pred=[53, 23, 30]  truth=[53, 23, 30]  d=0
  10  cavity    max_turns_exhausted     20    20  0.1373   no_answer  truth=[25, 32, 25]/r=3
  11  cavity    end_turn                12    11  0.0751   hit      pred=[36, 27, 34]/r=9.0  truth=[36, 27, 34]/r=9
  12  cavity    end_turn                12    11  0.0789   hit      pred=[29, 30, 35]/r=5.5  truth=[29, 30, 35]/r=5
  13  cavity    end_turn                 9     8  0.0627   hit      pred=[26, 34, 32]/r=7.0  truth=[26, 34, 32]/r=7
  14  cavity    max_turns_exhausted     20    20  0.1507   no_answer  truth=[28, 25, 38]/r=6
  15  cavity    end_turn                15    14  0.1397   hit      pred=[38, 31, 39]/r=4.0  truth=[38, 31, 39]/r=4
  16  cavity    max_turns_exhausted     20    20  0.0643   no_answer  truth=[38, 38, 37]/r=7
  17  cavity    end_turn                14    13  0.1517   hit      pred=[29, 37, 26]/r=7.0  truth=[29, 37, 26]/r=7
  18  cavity    end_turn                14    13  0.0998   hit      pred=[28, 27, 37]/r=9.0  truth=[28, 27, 37]/r=9
  19  cavity    end_turn                13    12  0.1024   hit      pred=[31, 32, 35]/r=4.5  truth=[31, 32, 35]/r=4
  20  blobs     end_turn                 6     5  0.0161   hit      pred_n=2  truth_n=2
  21  blobs     end_turn                 5     4  0.0154   hit      pred_n=3  truth_n=3
  22  blobs     end_turn                 5     4  0.0154   hit      pred_n=5  truth_n=5
  23  blobs     end_turn                 5     4  0.0143   hit      pred_n=3  truth_n=3
  24  blobs     end_turn                 5     4  0.0150   hit      pred_n=5  truth_n=5
  25  blobs     end_turn                 6     5  0.0168   hit      pred_n=2  truth_n=2
  26  blobs     end_turn                 5     4  0.0124   hit      pred_n=2  truth_n=2
  27  blobs     end_turn                 5     4  0.0129   hit      pred_n=3  truth_n=3
  28  blobs     end_turn                 5     4  0.0139   hit      pred_n=3  truth_n=3
  29  blobs     end_turn                 5     4  0.0137   hit      pred_n=3  truth_n=3
```

### 5.4 Tool histogram

```
  76  plot_over_line              ← workhorse for hotspots and cavity refinement
  40  find_connected_components   ← solved every blobs case + half the cavities
  31  get_available_arrays        ← always once per case
  31  get_histogram               ← always once per case
  30  load_data                   ← always once per case
   8  set_active_source           ← mostly cavity navigation
   5  get_screenshot              ← down sharply after the resolution fix
   3  create_isosurface
   2  create_slice, color_by, reset_camera
   1  rotate_camera, compute_surface_area, set_representation_type, get_pipeline
```

`find_connected_components` is now the second-most-used tool after only one
generation of use — an ablation study.

---

## 6. Observations

### 6.1 Hotspots are mechanically solved

Every hotspot case followed the same 5-call sequence:

```
load_data → get_available_arrays → get_histogram → plot_over_line → end_turn
```

The model uses the histogram to confirm the existence of an outlier
distribution, then a single line probe (sometimes followed by 1–2 refining
probes) along the right axis to triangulate. Hits to integer voxel were
unanimous. Average cost $0.017 / case.

### 6.2 Blobs are mechanically solved with the connectivity tool

Every blob case used the same 4–5 call sequence, almost without exception:

```
load_data → get_available_arrays → get_histogram → find_connected_components → end_turn
```

The model picks a threshold by reading the histogram (background ~0, peaks
≥1), then `find_connected_components(threshold=…)` returns the entire answer
in one call. Average cost $0.015 / case — actually *cheaper* than hotspots
because the conversation never has to grow past 5 turns.

This is the most decisive finding of the benchmark: **a single missing tool
turned a 25-call max-turns failure into a 4-call success**, with no change to
the model. Pre-fix the model could not solve any blobs case in 25 turns; post-
fix it solved 10 of 10 in an average of 4.2 calls.

### 6.3 Cavities are the only category with persistent failure

| outcome | count | typical signature |
|---:|---:|---|
| hit | 7 | converges in 9–14 calls; uses `plot_over_line` along three axes through midpoints, infers center + radius from zero-region extents |
| no_answer | 3 | hits 20-turn ceiling; produces NO `final_answer` text at all |

The 3 failures (cases 10, 14, 16) all have the same flavor: the model spreads
effort across many tools (plot_over_line + screenshots + isosurface +
create_slice + color_by + set_representation_type + ...) instead of focusing.
None of them produced any text output before the loop terminated — they kept
calling tools right up to the 20-turn cap.

Notably, the 3 failed cases all have **small radius** truths (3, 6, 7) where
line probing requires the model to find a precise transition point. The 7
successful cavities span radii 4–9, so radius alone doesn't predict failure
— it's a model focus issue, not a problem with the tool ergonomics.

### 6.4 Cost is dominated by failure cases

| category | total cost | share |
|---|---:|---:|
| 10 hotspots | $0.17 | 12% |
| 7 cavity hits | $0.71 | 51% |
| 3 cavity max_turns | $0.35 | 25% |
| 10 blobs | $0.15 | 11% |
| **total** | **$1.38** | **100%** |

The 3 unfinished cavity cases — 10% of the case count — eat 25% of the budget.

### 6.5 Prompt caching is doing the heavy lifting

```
input            =   4 932 tokens
cache_read       = 684 831 tokens
cache_create     = 165 049 tokens
output           =  44 339 tokens
```

Cache reads are 99% of the conversational input. The proxy passes
Anthropic prompt-caching headers through to its Vertex backend, so each turn's
system prompt + tool schemas + most of the message history gets billed at
cache rates (~10% of regular input). Without caching, the same workload would
have cost roughly 5× more. This was discovered by accident — my pre-run
estimate of $2–3 was based on no caching; the actual run came in at $1.38.

### 6.6 Tool histogram shifted dramatically vs the pre-fix baseline

Pre-fix 6-case sweep (sonnet_baseline) showed `plot_over_line` doing 50% of all
calls and `find_connected_components` not existing. Post-fix at 30 cases:

| tool | pre-fix share | post-fix share | comment |
|---|---:|---:|---|
| plot_over_line | ~54% | ~32% | still the workhorse, but no longer over-used |
| find_connected_components | n/a | ~17% | the new structural tool |
| get_screenshot | ~11% | ~2% | fixed PNG resolution + agent stopped reaching for it as a fallback |
| set_active_source | ~3% | ~3% | unchanged |

The model now uses fewer tools in total per case and uses the *right* tool
more often.

### 6.7 Proxy reliability is the operational risk

During the 30-case sweep the proxy returned two flavors of 422 errors that
killed runs without explicit retry handling:

- `No available accounts: no available accounts` — Vertex account pool
  exhausted, typically resets at top of hour.
- `Upstream service temporarily unavailable` — different wording, same
  symptom, the original retry list missed it.

Both are now matched by the broadened retry markers
(`no available account`, `rate`, `upstream`, `unavailable`, `temporarily`,
`timeout`). The 30-case sweep needed one resume run for cases 16-19 because
the second wording wasn't yet covered.

### 6.8 Two scorer false negatives traced to parser, not model

Two cavity cases initially scored as "partial" had the correct radius in the
final answer but in non-obvious phrasings:

- Case 13 wrote `**r ≈ 7 voxels**` (truth r=7) — parser was looking for
  `radius`, not `r`.
- Case 15 wrote `**≈ 4 voxels**` under a `Cavity Radius` header
  (truth r=4) — parser had no header-context fallback.

Both were rescored as hits after broadening `parse_radius`. The lesson:
**when the agent's output is free-form, the scorer is part of the benchmark
surface area**. False negatives in the parser look identical to model
failures and need to be hunted down separately.

---

## 7. Conclusions

1. **The agent can use the ParaView MCP properly.** 27 of 30 needle-hunt
   tasks were answered correctly using only MCP tools, no human in the
   loop, by a single model (`claude-sonnet-4-6`).

2. **Hotspots and blobs are essentially solved categories** (20/20 = 100%).
   The agent uses a stable, repeatable, minimal tool sequence on every case.
   For these categories the MCP toolset is sufficient.

3. **Cavities are 70% solved.** All 3 failures are model-side: the model fails
   to focus and produces no text answer before the turn budget runs out.
   None of the failures are tool-availability gaps. A `compute_region_extent`
   helper that returns a region's bounding-box dimensions and inscribed
   sphere would probably push cavities to ≥90% by removing the need for
   the model to construct radius estimates by hand from zero-region extents.

4. **The benchmark is a useful MCP design tool, not just an agent eval.**
   Running it surfaced 9 latent defects in the ParaView MCP, including one
   structural gap (`find_connected_components`) whose addition flipped a
   category from 0% to 100% hit rate. Running an unfinished MCP against an
   honest set of tasks is an effective way to find missing or broken tools.

5. **Per-correct-answer cost is ~$0.05.** The 30-case sweep cost $1.38 and
   produced 27 correct answers, so each correct answer cost $0.051. Cost is
   dominated by max-turns cases — fixing those (or capping max-turns more
   aggressively) is the largest single cost lever.

6. **The most important defect is also the most measurable one.** Pre-fix:
   the model achieved 0/2 on blobs in 25-turn runs at $0.13/case. Post-fix
   (with `find_connected_components`): 10/10 in 4-call runs at $0.015/case.
   That is a **9× cost reduction** and an **infinite hit-rate improvement**
   from a single ~150-line tool addition. This is the kind of finding the
   benchmark is supposed to produce.

---

## 8. Files produced

```
benchmark/
├── gen_needles.py                     # synthetic data generator
├── run_benchmark.py                   # Anthropic-API tool-use loop runner
├── summarize.py                       # transcript scorer
├── _check_mcp.py                      # tool listing sanity check
├── _check_pipeline.py                 # tool-only end-to-end smoke test
├── cases_30/                          # 30-case set used in this eval
│   ├── manifest.json
│   ├── case_000_hotspot.vti / .truth.json
│   ├── ... (30 cases total)
│   └── case_029_blobs.vti  / .truth.json
└── transcripts/
    ├── sonnet_30/                     # 30 transcripts, this eval
    ├── sonnet/                        # 6-case post-fix sweep
    ├── sonnet_baseline/               # 6-case pre-fix sweep (preserved)
    └── opus/                          # 2-case opus comparison
```

## 9. Reproducibility

```bash
# 1) start pvserver in a restart loop
PVDIR=/mnt/common/hxu40/spack/opt/spack/linux-skylake_avx512/paraview-5.13.3-ssmv5hp4czyfvuu5eps6s2ljpug7lkus/bin
while true; do
  $PVDIR/pvserver --multi-clients --server-port=11111
  sleep 0.3
done &

# 2) generate cases
PVPY=$PVDIR/pvpython
$PVPY benchmark/gen_needles.py --per-type 10 --out benchmark/cases_30

# 3) run the sweep
ANTHROPIC_BASE_URL=https://yxai.anthropic.edu.pl \
ANTHROPIC_AUTH_TOKEN=<token> \
$PVPY benchmark/run_benchmark.py \
  --manifest benchmark/cases_30/manifest.json \
  --out      benchmark/transcripts/sonnet_30 \
  --model    claude-sonnet-4-6 \
  --max-turns 20

# 4) score
$PVPY benchmark/summarize.py \
  --transcripts benchmark/transcripts/sonnet_30 \
  --cases       benchmark/cases_30
```

---

## 10. Suggested follow-ups

1. **Add `compute_region_extent(threshold)`** that returns each connected
   region's bounding box and inscribed-sphere radius. Cheapest path to fixing
   the 3 unsolved cavities.
2. **Tighten `--max-turns`** from 20 to 12 for hotspots/blobs (no successful
   case in those categories used more than 6 calls). Caps the cost ceiling on
   any future failures.
3. **Run claude-opus-4-6 on the same 30-case set** for a model comparison.
   Estimated cost ~$8 with caching; the existing 2-case opus sample showed
   opus solving a cavity sonnet missed but at ~6× the per-case cost.
4. **Add a "no needle" control case** to the generator: pure background noise.
   The model should report "no anomaly detected" rather than hallucinate a
   coordinate. This is the single most diagnostic test of agent honesty
   and isn't covered by the current case set.
5. **Add more needle types**: vector-field rogue streamlines, multi-modal
   distributions, cases with the wrong array name (typo) to test
   error-recovery, cases with corrupt headers to test failure handling.
