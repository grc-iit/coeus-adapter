[← COEUS-Adapter README](../README.md) · [Trigger-Render-Reason](TRIGGER_RENDER_REASON_PIPELINE.md) · [Build & Run (single node)](BUILD_AND_RUN_GRAY_SCOTT.md)

# Artifact Description - Vigil Trigger-Render-Reason at Scale on NCSA Delta

This document is a self-contained **Artifact Description (AD)** for reproducing the
Vigil *trigger–render–reason* in-situ pipeline on **NCSA Delta**, including the
**multi-node scale run** (256-rank producer + AI-agent consumer). It is written to
be followed top-to-bottom by an evaluator with a Delta allocation.

> **What the artifact demonstrates.** A running Gray-Scott simulation steers
> *itself*: the COEUS/hermes engine computes a global **variance(V)** statistic at
> every output step (from ADIOS2 derived variables), detects the *pattern-collapse*
> event, and streams **only the flagged window** over ADIOS2 **SST** to a ParaView
> **MCP** AI agent on a separate node. The agent renders the streamed steps
> headlessly, *sees* that the field has homogenised, and calls the MCP tool
> `fire_stop_simulation`, which halts the 256-rank run early - saving the remaining
> compute with no loss of science. The trigger statistic is **rank-invariant**:
> it is bit-identical at 1, 4, and 256 ranks.

---

## 1. Artifact check-list (metadata)

| Item | Value |
|---|---|
| Program | COEUS-Adapter (`hermes_engine` ADIOS2 plugin) + `adios2-gray-scott` |
| Compilation | CMake + GCC 13.3.1; Spack-provided deps |
| Run-time env | IOWarp/clio-core runtime, ADIOS2 2.11 (SST + derived vars), ParaView 5.13.3 (OSMesa), Python `mcp`/`anthropic` |
| Hardware | NCSA Delta CPU nodes - 2× AMD EPYC 7763 (128 cores/node), Slingshot-11 (`hsn0`) |
| Orchestration | Jarvis-CD pipeline + `srun`/PMIx (Delta denies inter-node ssh) |
| Model | Anthropic `claude-haiku-4-5` (any Claude model works; needs an API key) |
| Metrics | trigger fire step + value; agent verdict; early-halt step; wall-time; \$ cost |
| Output | `trigger_log.jsonl`, rendered PNGs, agent `token_usage.json` + screenshots |
| Approx. run time | build ≈ 2–3 h (ParaView is the long pole); one scale run ≈ 5 min |
| Public repo | `https://github.com/grc-iit/coeus-adapter` (branch used here: `iowarp_2`) |

---

## 2. Hardware & allocation

Delta CPU nodes have 128 physical cores and the Slingshot-11 high-speed NIC
`hsn0`. The scale run below uses **3 nodes in one SLURM job**: two producers
(256 ranks, 128/node) and one consumer (SST reader + AI agent).

```bash
# One 3-node job (2 producer + 1 consumer). --exclusive => whole nodes.
salloc --account=<your-cpu-account> --partition=cpu \
       --nodes=3 --ntasks-per-node=128 --time=08:00:00 --exclusive
```

After it starts, record the node names - everything below assumes them:

```bash
squeue -u $USER                      # NODELIST e.g. cn[024,046,071]
scontrol show hostnames $(squeue -u $USER -h -o '%N')
#   cn024  cn046   <- producers
#   cn071          <- consumer
```

> **Delta specifics baked into this artifact (see §7).** Compute nodes deny ssh
> **even to localhost/self** (`Permission denied (hostbased)`), and `mpirun`
> cannot span a multi-node allocation here. All launches therefore go through
> **`srun`** inside the job. These are handled automatically by the provided
> scripts; §7 explains the mechanism for reviewers.

---

## 3. Software environment (Spack)

All dependencies come from a user Spack tree (`$HOME/spack`). Delta has no site
adios2/hermes modules. Install once:

```bash
source $HOME/spack/share/spack/setup-env.sh

# IOWarp/clio-core runtime (Chimaera + Context-Transfer-Engine)
spack install --fresh iowarp@main ~adios2 ^openmpi@5.0.10

# ADIOS2 2.11 + COEUS derived variables (variance/mean/gradient/spectrum),
# with SST. Must be +pic +shared (shared only exists when +pic).
spack install adios2-coeus@vigil +pic +shared ^openmpi@5.0.10

spack install openmpi@5.0.10 googletest sqlite

# Headless ParaView for the RENDER stage. Delta CPU nodes have NO GPU (no EGL)
# and NO X server, so rendering needs OSMesa (software GL). This build is ~x
# (native offscreen), embeds adios2@2.11 (SST-compatible with the writer),
# keeps the ParaView 5.13 Fides API the reader scripts target, and its pvpython
# imports mcp+anthropic. (mesa~glx+osmesa provides libOSMesa.)
spack install paraview@5.13.3 +adios2+fides+mpi+python~qt ^mesa~glx+osmesa

# Python packages for the agent + MCP server (into the Spack python 3.12 that
# ParaView's pvpython uses).
python3 -m pip install --user "mcp[cli]" httpx anthropic openai
```

Record the ParaView hash - the reader is loaded **by hash** because two
`paraview@5.13.3` installs can coexist (only the `+osmesa`/`~x` one renders
headlessly):

```bash
spack find -lv paraview   # note the hash of the '~x ... ^...osmesa' spec
# e.g.  7fqo3o7 paraview@5.13.3 ... ~qt ... ~x   <-- use THIS hash
```

The **ParaView MCP** base (`paraview_manager.py`) is expected at
`$HOME/software/paraview_mcp` (`git clone https://github.com/LLNL/paraview_mcp`).

**Anthropic API key** (for the *reason* stage): `export ANTHROPIC_API_KEY=sk-ant-...`.

---

## 4. Build COEUS-Adapter

```bash
git clone https://github.com/grc-iit/coeus-adapter
cd coeus-adapter && git checkout iowarp_2

# Loads the Spack deps + puts build/bin on PATH/LD_LIBRARY_PATH and sets the
# multi-node env (PMIX_MCA_gds=hash, TMPDIR) - see the file.
source CI/Delta/env.sh

mkdir -p build && cd build
cmake ..
make -j8
```

Products in `build/bin/`: `adios2-gray-scott`, `libhermes_engine.so`, and the
`libcoeus_*_runtime.so` ChiMods. `CI/Delta/env.sh` already exports
`ADIOS2_PLUGIN_PATH`, `LD_LIBRARY_PATH`, and `PATH` for these.

---

## 5. One-time Jarvis setup

The pipeline is orchestrated with [Jarvis-CD](https://github.com/grc-iit/jarvis-cd).

```bash
cd /path/to/coeus-adapter
source CI/Delta/env.sh

# Register the coeus + clio-core jarvis package repos (paths as installed).
jarvis repo add   $PWD/test/jarvis/jarvis_coeus
jarvis repo add   /path/to/clio-core/jarvis_clio_core

# Jarvis hostfile = the PRODUCER nodes only (edit to your 2 producer nodes).
printf "cn024\ncn046\n" > $HOME/producer_hostfile
jarvis hostfile set $HOME/producer_hostfile
```

**Edit the two producer/consumer bindings to your allocation's node names**
before loading the pipeline:

- `test/jarvis/jarvis_coeus/pipelines/vigil/gray-scott/delta/gray-scott-warn-mn.yaml` →
  `srun_nodelist: "cn024,cn046"` (your producers) and the absolute path of
  `CI/Delta/jarvis-ssh-local-shim.sh` in `ssh_cmd`/`pssh_cmd`.
- `CI/Delta/run_gs_multinode.sh` → `PRODUCER_NODES`, `CONSUMER_NODE`, and
  `PV_HASH` (the OSMesa ParaView hash from §3).

Load and build the pipeline environment:

```bash
jarvis ppl load yaml test/jarvis/jarvis_coeus/pipelines/vigil/gray-scott/delta/gray-scott-warn-mn.yaml
jarvis ppl env build
jarvis ppl print | grep -E "nprocs|ppn|launcher|srun_nodelist|steps|Hosts"
#   Hosts: cn024, cn046
#   nprocs: 256   ppn: 128   launcher: srun   srun_nodelist: cn024,cn046   steps: 20000
```

---

## 6. Run

### 6a. Single-node smoke test (no API key) - validate the toolchain

Confirms trigger → gated SST → headless render on one node (writer + reader on
the same node). Renders the 4 flagged frames, no LLM:

```bash
jarvis hostfile set $HOME/producer_hostfile   # (single node is fine too)
jarvis ppl load yaml test/jarvis/jarvis_coeus/pipelines/vigil/gray-scott/delta/gray-scott-warn.yaml
jarvis ppl env build
MODE=render bash CI/Delta/run_gray_scott_agent.sh
#   -> $HOME/iowarp/output-0000{0..3}.png  and a trigger_warning in trigger_log.jsonl
```

Add the agent (full loop, single node) with a key:

```bash
MODE=agent ANTHROPIC_API_KEY=sk-ant-... bash CI/Delta/run_gray_scott_agent.sh
```

### 6b. Multi-node scale run (256-rank producer + agent consumer)

This is the headline result. One command orchestrates everything from the
producer login node inside the allocation:

```bash
source CI/Delta/env.sh                        # sets SLURM env + PMIX_MCA_gds=hash
export ANTHROPIC_API_KEY=sk-ant-...
bash CI/Delta/run_gs_multinode.sh
```

`run_gs_multinode.sh` does, in order:

1. `jarvis ppl run` - starts the clio runtime + CTE **on both producer nodes**
   (the ssh shim launches the remote daemon via `srun`), then launches the
   **256 `adios2-gray-scott` ranks** across the producers with
   `srun --mpi=pmix`. The writer blocks at the SST rendezvous.
2. Waits for the SST contact file `$HOME/iowarp/gs.bp.sst` (on NFS).
3. `srun --nodelist=<consumer>` runs `CI/Delta/gs_consumer.sh` on the consumer
   node: **pvserver** (headless OSMesa) ← **bridge** (`insitu_streaming.py`) ←
   **ParaView MCP server** (`insitu_mcp_server.py`) ← **agent**
   (`insitu_agent.py`). The agent inspects the streamed collapse window and
   calls `fire_stop_simulation`.
4. The writer detects `$HOME/iowarp/gs_warn.bp.stop` and halts early.

---

## 7. How the multi-node launch works (for reviewers)

Delta forbids node-to-node ssh, and `mpirun` will not pick up the allocation.
Three mechanisms make the run work **entirely through `srun`** (no ssh):

1. **`srun`+PMIx for the MPI ranks.** The producer launches with
   `PMIX_MCA_gds=hash srun --jobid=$SLURM_JOB_ID --mpi=pmix -N2
   --ntasks-per-node=128 -n256 --nodelist=<producers> --overlap
   adios2-gray-scott …`. `--mpi=pmix` is required (`pmi2` yields singletons);
   `PMIX_MCA_gds=hash` works around a Slurm PMIx-v5 shmem-GDS open failure; all
   binaries/configs live on **NFS** (`$HOME`). This is the `launcher=srun` path
   in `jarvis_coeus/adios2_gray_scott/pkg.py` and the env exports in
   `CI/Delta/env.sh`.
2. **ssh shim for the clio daemons.** Jarvis starts one runtime/CTE daemon per
   producer node via `pssh`. `CI/Delta/jarvis-ssh-local-shim.sh` (wired as the
   pipeline's `ssh_cmd`/`pssh_cmd`) runs **local-host** launches directly and
   routes **remote** hosts through `srun --overlap` - so the daemon starts on
   the second producer node without ssh.
3. **Cross-node SST over NFS + TCP.** The gated SST stream is WAN/TCP; the
   consumer reads the writer's contact file `gs.bp.sst` from the shared
   filesystem and connects over `hsn0`. Producer and consumer need no ssh
   between them.

Rendering is **OSMesa software GL** (CPU nodes have no GPU/EGL and no X); the
ParaView build in §3 renders offscreen natively. The MCP server + agent run
under the Spack **python** (has `mcp`+`anthropic`) with the OSMesa ParaView on
`PYTHONPATH`, so the MCP client version matches the pvserver.

---

## 8. Expected results

Two verified reference runs, 2026-07-17 on Delta (job across `cn[024,046,071]`),
256 producer ranks, `F=0.08`, `k=0.03`, `plotgap=50`, `steps=20000`,
`claude-haiku-4-5`:

**Scale reference (L=256, 32-rank parallel consumer)** - the headline config:
warn at output **125** (`variance 5.61e-4` = 12× collapse vs baseline
`4.66e-5`; the 64×-smaller baseline vs L=64 confirms the 1/L³ scaling), agent
started only after the warn and fired **43 s** later ("V field expanded to
uniform bulk - collapsed to homogeneous state"), writer halted early at step
**6500** of 20000 (67% skipped). Agent: 10 tool calls, 4 screenshots, ~39 s,
≈ \$0.08.

**Mechanics reference (L=64, single consumer)** - fast smoke config:

**Trigger** - `$HOME/iowarp/writer_mn.log` and the fire log
(`…/executions/<id>/shared/jarvis_coeus.adios2_gray_scott/trigger_log.jsonl`):

```
Trigger WARNING at step 24: variance(derive/VarV) collapsed to 0.025142
    (baseline 0.00296, arm 20x, collapse 13x); pattern expanding to blank -
    streaming 4 step(s) to the agent for the fire verdict
{"event":"trigger_warning","step":24,"variable":"derive/VarV",
 "value":0.025142,"baseline":0.00295759,"arm_ratio":20,"collapse_ratio":13}
```

The value `0.025142` is **bit-identical** to the 1- and 4-rank runs - evidence
that the pooled global variance is decomposition-invariant.

**Reason** - the agent (`test/insitu_agent/agent_results/`):

```
FIRE verdict: wrote halt flag gs_warn.bp.stop
    (reason="V isosurface vanished; field uniform ~0.59 - pattern saturated")
token_usage.json: 11 LLM calls, 10 tool calls, 4 screenshots, ~220 s, ≈ $0.08
```

`~0.59` matches the analytic Gray-Scott steady state `V* ≈ 0.592` - the model
read the physics off the rendered frames.

**Early halt** - `writer_mn.log`:

```
Simulation halting early at step 1600: trigger STOP flag detected
```

i.e. the 256-rank run stops at output ~32 instead of 400 (`steps=20000`) - the
field is stationary at the analytic fixed point from output ~28 on, so every
later output is redundant.

Rendered frames: `$HOME/iowarp/output-0000{0..3}.png` (render mode) or the
agent's `agent_results/screenshots/agent_000{1..4}.png` (agent mode) - a
V-isosurface shrinking into a uniform field across the window.

---

## 9. Scaling knobs (for the paper's scaling study)

All are pipeline parameters - set in `gray-scott-warn-mn.yaml` or via
`jarvis pkg configure coeus-gray-scott.jarvis_coeus.adios2_gray_scott <k=v> …`
then `jarvis ppl env build`:

| Knob | Meaning | Notes for scaling |
|---|---|---|
| `nprocs`, `ppn` | total ranks, ranks/node | e.g. 512 ranks = `ppn=128` on 4 producer nodes; grow the salloc + `producer_hostfile` + `srun_nodelist` accordingly |
| `L` | grid size (L³) | weak-scale L with rank count; the trigger uses **ratios** so its arm/collapse levels are L-independent |
| `steps`, `plotgap` | total steps, output cadence | `steps` must be large enough that the writer outlives the agent's decision latency (the early-halt is only visible if the sim is still running when the agent fires). `plotgap=50` is part of the trigger calibration - do not change without recalibrating `trigger_baseline_ratio`/`trigger_collapse_baseline_ratio` |
| `trigger_baseline_ratio` / `trigger_collapse_baseline_ratio` | arm / warn levels | 20× / 13× for F=0.08,k=0.03,plotgap=50; the **ratios** are L-portable (verified: L=256's baseline is 64× smaller - the 1/L³ scaling - and the same 20×/13× ratios warned correctly) |
| `trigger_inspect_steps` | flagged steps per fire | window the agent inspects (default 4) |
| `CONS_NPROCS` | SST reader parallelism | Parallel pvserver ranks on the consumer node (default 32, verified at 256→32). **Must be launched with `mpirun` (local fork), never `srun --mpi=pmix`** - srun-launched reader cohorts hang the SST N-to-M rendezvous (the orchestrator does this correctly; it also scrubs `SLURM_*` env inside the step so mpirun's slurm RAS doesn't refuse `-np N`). Keep all reader ranks on **one node** - cross-node readers hang in IceT compositing |
| `PRODUCER_NODES`, `CONSUMER_NODE`, `srun_nodelist`, jarvis hostfile | node placement | **update all of these to your allocation's node names** |

**Warn timing vs. L.** The pattern grows from a fixed 12³ seed, so the time to
fill (and then homogenise) the domain scales ~linearly with L: the collapse-warn
fires at output ~24 at L=64 but output ~125 at L=256 (verified). Budget `steps`
accordingly (≥ `plotgap × 1.5 × 24·(L/64)`), and note the RAM CTE tier must hold
all outputs to the halt (no eviction tier configured): at L=256 that is
0.25 GB/output/node - 125 outputs ≈ 31 GB, just inside the 32 GB tier.

**Known limitation - L=512 producer-side stall (ROOT-CAUSED 2026-07-18).** At
L=512 the writer's 4 MB-per-rank blob Puts stall inside the CTE (daemon intake
plateaus, a subset of ranks spin in clio `Wait()`, the rest block in the next
collective); L=256 (0.5 MB blobs) runs cleanly with the same 256 ranks.
`client_data_segment_size=8G`/`num_threads=8` delays but does not fix it.

The stall was **reproduced and diagnosed on Ares** (15 nodes × 40 cores,
512 ranks, 2026-07-18) with the identical signature - output interval grew
27 s → 110+ s while daemon RSS climbed without bound, ranks caught in gdb
spinning in `Future::Wait` inside `EndStep → ComputeDerivedVariables →
CTETagClient::Get`, one rank in the trigger `MPI_Allreduce`, the rest piled
into the next halo exchange. The cause is a four-factor chain, not a race:

1. **CTE placement scatters every blob.** `HashBlobToContainer()`
   (clio-core `context-transfer-engine/core/src/core_runtime.cc`, used by
   `Route()` for all PutBlob/GetBlob) maps each blob to a **uniformly random
   node** via `DirectHash(hash(tag, blob_name))`. With N producer nodes,
   (N-1)/N of all output bytes cross the network (Delta 2 nodes: 50%;
   Ares 15 nodes: ~93%). `neighborhood: 1` does **not** localize placement -
   it only pins each container's storage target to its own bdev.
2. **The engine reads everything back at every output.**
   `HermesEngine::ComputeDerivedVariables()` (`src/hermes_engine.cc`) calls
   `hermes_->tag->Get(source)` for each source of each of the **4** derived
   variables (AddU/AddV/VarU/VarV) - per rank per output that is a 4 MB Put
   (U+V) plus an **8 MB Get-back** (U ×2, V ×2), again hash-scattered.
3. **The daemon↔daemon ZMQ mesh (port 9413) is the choke point.** Measured on
   Ares 1 GbE: all-to-all streams pinned at `cwnd:10` (~14 KB in flight),
   RTT inflated 14-22 ms by the co-resident MPI halo traffic, ~1 MB/s per
   stream, 300-460 KB Send-Q backlogs - aggregate drain far below the
   ~6 GB/output intake, so the backlog (and each output's latency) grows
   monotonically until it presents as a stall.
4. **The per-output trigger `Allreduce` couples the cohort** to the slowest
   rank's CTE ops, so one slow Get stalls all ranks.

Since bytes/output ∝ L³ at fixed rank count, this is exactly why **blob size,
not rank count**, is the trigger, and why L=256 stays clean (A/B-verified on
Ares: L=256/512 ranks = 5.8 s/output flat, completed; L=512/512 ranks on the
same nodes degraded 27→110+ s/output and never finished).

**Potential fix** (in order of leverage):

- **Operational, verified:** put the CTE daemon mesh (and MPI) on the fastest
  fabric. On Ares, moving clio's `networking.hostfile` to the 40 GbE hostnames
  + `net_if=<40G NIC>` turned the same L=512/512-rank run from a degrading
  stall into a **steady ~28.6 s/output, completing 40/40 outputs in 1160 s**
  (streams idle between outputs, delivery 0.8-87 Gbps when active). On Delta,
  confirm the daemon hostfile resolves to `hsn0` addresses - if the daemons
  inter-connect over the management network, the same collapse follows. (Ares
  gotcha: seed `known_hosts` for the alternate hostnames with `ssh-keyscan` -
  the mpirun wrapper's Spack openssh fails as a misleading "PRTE has lost
  communication with a remote daemon".)
- **CTE fix (root):** add a local-first placement mode - route `PutBlob` to
  the writer's local container (`PoolQuery::Local`) instead of
  `DirectHash(hash)`. Simulation output has no reason to leave the node at
  Put time; DHT-style scatter can remain opt-in for shared/global tags.
- **Engine fix (removes ~2/3 of the traffic):** compute derived variables
  from the in-memory data the rank just Put instead of `Get`-ing the blobs
  back in `ComputeDerivedVariables()` (cache the Put buffers for the step, or
  Get once per source variable instead of once per derived variable).
- **Diagnosis aid:** under fabric congestion rank-0 stdout arrives *minutes*
  late through the launcher (13+ outputs behind on Ares) - a silent writer is
  not necessarily stalled. Ground-truth progress = CTE step tags
  (`cte_search ".*" "step.*"`) or `ss -tin` byte counters on port 9413.

Until the CTE/engine fixes land, the verified maximum for the full loop on
Delta is **L=256 with 256 ranks**; on Ares, **L=512 with 512 ranks completes
when the daemon mesh runs on the 40 GbE fabric**. The 64-rank L=512 Ares
reference in [BUILD_AND_RUN_GRAY_SCOTT.md](BUILD_AND_RUN_GRAY_SCOTT.md) §3
predates this diagnosis.

Baseline (no-agent, engine-only) and larger-`L`/rank reference points from Ares
are tabulated in [BUILD_AND_RUN_GRAY_SCOTT.md](BUILD_AND_RUN_GRAY_SCOTT.md) §3.

---

## 10. Troubleshooting

| Symptom | Cause / fix |
|---|---|
| `Permission denied (hostbased)` during jarvis | ssh shim not wired - check `ssh_cmd`/`pssh_cmd` in the loaded `pipeline.yaml` point to `CI/Delta/jarvis-ssh-local-shim.sh` |
| `prterun was unable to find … adios2-gray-scott` | `build/bin` not on the **captured** PATH - `source CI/Delta/env.sh` then re-run `jarvis ppl env build` |
| `PMIX_ERR_FILE_OPEN_FAILURE … gds_shmem2` / ranks become singletons | need `PMIX_MCA_gds=hash` and `--mpi=pmix` - set by `env.sh` + the pkg's srun path; ensure binaries are on NFS |
| Reader/pvbatch aborts with `bad X server connection` | wrong ParaView - use the **`+osmesa`/`~x`** build (load by hash), not the `+x` one |
| Writer runs to completion (no early halt) | `steps` too small vs. agent latency (§9), or the agent kept calling `advance_step` past the window and blocked - the provided prompt fires after `trigger_inspect_steps` frames |
| Rendezvous never reached | start a **fresh** run (stale `gs.bp.sst`), and confirm the clio daemons came up on both producers (`writer_mn.log` shows `IOWarp runtime started` + `CTE started`) |

For the single-node walkthrough, deeper trigger semantics, and the Ares
reference numbers, see
[BUILD_AND_RUN_GRAY_SCOTT.md](BUILD_AND_RUN_GRAY_SCOTT.md) and
[TRIGGER_RENDER_REASON_PIPELINE.md](TRIGGER_RENDER_REASON_PIPELINE.md).
