# Building COEUS-Adapter and Running Gray-Scott (clio-core backbone)

Verified end-to-end on Ares, 2026-07-09 (writer: `ares-comp-11`, reader:
`ares-comp-14`). The trigger-gated SST mode (Section 3) was verified the same
day at L=64 and L=512 (64 ranks across ares-comp-11/14/15/16), and again
2026-07-11 at L=256 with 32 ranks on ares-comp-18/20 and the reader on
ares-comp-22 (see the worked example at the end of Section 3).

## 1. Build COEUS-Adapter

### Prerequisites (Spack)

The backbone is **clio-core**, installed as the `iowarp` Spack package
(Chimaera runtime + Context-Transfer-Engine). ADIOS2 comes from the
`adios2-coeus` package (`vigil` branch: Derived variables + SST enabled).

```bash
spack load iowarp@main
spack load adios2-coeus@vigil
```

### Configure and build

```bash
cd coeus-adapter
mkdir -p build && cd build
cmake ..
make -j8
```

Notes:

- `find_package(iowarp-core)` resolves through the loaded `iowarp@main`
  install; `ADIOS2_DIR` resolves to the loaded `adios2-coeus@vigil`.
- Catalyst/Fides in-situ support is enabled automatically when CMake finds a
  Catalyst install (`COEUS_HAVE_CATALYST`); the `COEUS_ENABLE_CATALYST` option
  is only needed to force it on without a find.
- Optional flags: `-Dmeta_enabled=ON` (metadata collection),
  `-Ddebug_mode=ON`.

Build products in `build/bin/`:

| Artifact | Purpose |
|---|---|
| `libhermes_engine.so` | The ADIOS2 plugin engine (I/O → CTE) |
| `libcoeus_coeus_mdm_runtime.so`, `libcoeus_rankConsensus_runtime.so` | ChiMods loaded by the clio runtime |
| `adios2-gray-scott` | Gray-Scott simulation binary |

## 2. Run Gray-Scott through the Jarvis pipeline

The pipeline `coeus-gray-scott` (loaded from
`test/jarvis/jarvis_coeus/pipelines/gray-scott.yaml`, materialized in
`/mnt/common/hxu40/jarvis-pipelines/coeus-gray-scott/`) has three stages:

1. `clio_runtime` — Chimaera runtime daemon (ZMQ port 9413)
2. `clio_cte` — composes the CTE core pool (`clio_cte_core`, pool 512.0,
   32 GB RAM tier) via `clio_run compose`
3. `adios2_gray_scott` — 16 MPI ranks, L=128, 150 steps, output every 10
   steps, `engine=hermes` (the COEUS plugin)

### Step A — launch the pipeline (writer side)

```bash
spack load iowarp@main adios2-coeus@vigil
jarvis ppl kill    # clean up any previous deployment
jarvis ppl run
```

`jarvis ppl print` shows the configuration.

#### Multi-node: TWO hostfiles must both list all nodes

Verified 2026-07-09 with 32 ranks, 16/node on ares-comp-11 + ares-comp-14.
Jarvis uses **two different hostfiles**, and scaling to more nodes requires
updating **both**:

| Hostfile | Who uses it |
|---|---|
| Pipeline hostfile — `/mnt/common/hxu40/jarvis-pipelines/coeus-gray-scott/hostfile` (shown by `jarvis ppl print`) | Runtime + CTE stages (pssh: one clio daemon per node) |
| **Jarvis global hostfile** — `/home/hxu40/server_list/server.list` (set in `~/.ppi-jarvis/jarvis_config.yaml`, managed by `jarvis hostfile set`) | **The application's mpirun** — the `adios2_gray_scott` package passes `self.jarvis.hostfile` to `MpiExecInfo` |

If the global hostfile lists fewer nodes than `nprocs/ppn` needs, OpenMPI
aborts with:

```
Your job has requested more processes than the ppr for this topology can support:
  Number of procs: 32 ... PPR: 16:node
```

because the node pool comes from the global file only. The symptom is jarvis
"finishing" in seconds with the app stage failing.

**Node placement**: mpirun fills the global hostfile in order at `ppn`
ranks per node, so with `nprocs=32 ppn=16` the producer lands on the first
two listed nodes; the remaining nodes only run a clio daemon (cheap) and
are free for the reader. Run the SST reader (Step B) on any node NOT
hosting producer ranks. The runtime/CTE stages start one daemon on every
pipeline-hostfile node regardless.

#### OpenMPI remote launch: normal machines vs. Ares

Multi-node mpirun means OpenMPI must start a daemon (`prted`) on every remote
node over non-interactive ssh, and the daemons must talk back to `mpirun`
over TCP. What you need depends on the machine:

**Normal machine condition** (MPI on a system-wide PATH — distro package,
`module load` in shell init, or a scheduler-integrated MPI):

- Nothing extra. `ssh <node> prted` resolves, OpenMPI picks a working
  interface, and jarvis's default launch works as-is once both hostfiles
  (table above) list all nodes.
- Under SLURM/PBS, prefer running inside the allocation and letting OpenMPI
  use the scheduler's launcher (no ssh at all).

**Ares condition** (this cluster: OpenMPI 5 + PRRTE installed via Spack in
the user's home, multiple NICs):

- **Problem 1 — `prted: command not found`**: Spack's OpenMPI is not on the
  PATH of non-interactive ssh shells, so plain `mpirun --hostfile ...` dies
  spawning remote daemons (or silently drops nodes → the ppr error above).
  Jarvis partially compensates by deriving `--prefix <prrte-root>` from the
  pipeline env. The robust, verified fix is the repo's ssh wrapper, which
  injects the pre-resolved Spack PATH on the remote side
  (`test/real_apps/gray-scott/ssh-spack-wrapper.sh`). Wire it in as a
  top-level launcher override in the loaded pipeline's `pipeline.yaml`
  (`jarvis-pipelines/pipelines/coeus-gray-scott/pipeline.yaml`) — jarvis
  forwards top-level `mpi_cmd`/`ssh_cmd`/`pssh_cmd` to every launch:

  ```yaml
  mpi_cmd: mpirun --mca plm_rsh_agent /mnt/common/hxu40/coeus/iowarp/coeus-adapter/test/real_apps/gray-scott/ssh-spack-wrapper.sh --mca plm_ssh_no_tree_spawn 1
  ```

- **Problem 2 — wrong network interface**: Ares nodes have several NICs;
  OpenMPI picking the wrong one shows up as
  `PRTE has lost communication with a remote daemon`. Pin daemon and data
  traffic to `eno1` (the `adios2_gray_scott` package already exports these):

  ```bash
  export OMPI_MCA_pml=ob1
  export OMPI_MCA_btl=tcp,self
  export OMPI_MCA_osc='^ucx'
  export OMPI_MCA_btl_tcp_if_include=eno1
  export OMPI_MCA_oob_tcp_if_include=eno1
  ```

- If the Spack OpenMPI install moves (rebuild/new hash), regenerate the
  hardcoded paths inside `ssh-spack-wrapper.sh`.

**Debugging launch problems on either machine**: jarvis never echoes its
mpirun command. Point `mpi_cmd` at a small shim script that logs `argv`/env
to a file and then `exec`s the real mpirun — that is how the hostfile
mismatch above was found. Verified working reference (Ares, 2026-07-09):
64 ranks, 16/node across ares-comp-11/14/15/16.

### Step B — launch the SST reader (REQUIRED, or the pipeline hangs)

The pipeline's `adios2.xml` sets `CatalystStream=gs.bp`, so during engine
init every rank opens a Catalyst **SST writer with
`RendezvousReaderCount=1`**: all 16 ranks block inside `SSTIO->Open()`
(spinning at ~100% CPU) until an external Fides/ParaView SST reader
connects. The pipeline contains no reader package — start it manually,
typically on another node:

```bash
spack load paraview          # 5.13.3
cd coeus-adapter/test/real_apps/gray-scott
pvbatch catalyst/gs-pipeline.py \
    -j $PWD/catalyst/gs-fides.json \
    -b /mnt/common/hxu40/coeus/iowarp/coeus-adapter/gs.bp \
    --staging --num-steps 15
```

- The SST contact file `gs.bp.sst` is created in the **simulation's working
  directory** (the coeus-adapter repo root, NFS-shared) once the writer
  reaches rendezvous. `-b` must be the absolute path next to that file.
- Order: start the pipeline first (writer creates `gs.bp.sst` and waits),
  then the reader.
- The reader saves one `output-NNNNN.png` per received step. With the
  default ungated stream (`QueueFullPolicy=Discard`, `QueueLimit=1`) a slow
  reader skips steps rather than stalling the simulation.
- In **trigger-gated mode** (Section 3) set `--num-steps` to
  `trigger_inspect_steps` (default 3): the reader receives nothing until the
  trigger fires, then exactly that many steps, then exits on its own.

### Expected output

Writer (`jarvis ppl run` stdout): consensus-rank assignment for all 16
ranks, `Catalyst SST stream: gs.bp (multi-node)`, then per output step

```
Simulation at step 10 writing output step 1
[Coeus engine] SST in-transit step 1: Put time (us): ..., EndStep time (us): ...
```

through step 150 (output 15), a `ckpt.bp` checkpoint at step 70, then
`DoClose` messages and a clean jarvis exit.

## 3. Trigger-gated SST (Vigil trigger phase)

By default every output step streams over SST. With the **statistical
variance trigger** enabled, the engine watches the simulation at every output
step and ships **only flagged steps** — the "trigger" stage of the Vigil
trigger-render-reason pipeline. Untriggered steps cost one cheap variance
evaluation and ship nothing; the SST reader simply waits until a flagged step
arrives.

### Mechanism

1. **Derived quantities as trigger inputs.** With `engine=hermes_derived`
   the gray-scott writer declares ADIOS2 derived variables:
   `derive/VarV = variance(V)` and `derive/AddV = add(V)` (and the `U`
   counterparts). ADIOS2 evaluates them per **writer block** (one value per
   rank) at every output step, in the same address space as the simulation —
   no extra read of the field data.
2. **Exact global pooling.** A per-block variance is not a global variance.
   At `EndStep` the engine pools the blocks into the exact global variance:
   each rank reads its block variance (`derive/VarV`) and block sum
   (`derive/AddV`) from its CTE blob, then a single 3-double `Allreduce`
   combines them (see `VARIANCE_TRIGGER.md` §6; per-block variances are never
   averaged). If `TriggerVariable` names a raw field (e.g. `V`), the engine
   falls back to one local pass over the raw blob instead — no derived
   variables needed.
3. **Fire condition (rising edge).** The trigger fires on the **first upward
   crossing** of either condition (they are OR'd):
   - `variance >= TriggerThreshold` (absolute), or
   - `variance >= TriggerBaselineRatio ×` the first-output-step baseline.
   One fire per run unless `TriggerRefire=true`.
4. **On fire.** The firing step plus the next `TriggerInspectSteps - 1`
   output steps are re-Put from the CTE blobs to the Catalyst SST stream at
   `EndStep`. Each shipped step carries `vigil/trigger_fired`,
   `vigil/trigger_stat`, and `vigil/trigger_fire_step` scalars for a
   downstream agent, and rank 0 appends a JSON-lines fire event to
   `TriggerLogFile`. While gated, `QueueFullPolicy` defaults to `Block` so
   flagged steps are never dropped.

Physically, variance(V) tracks pattern formation: it sits at a flat baseline
during the perturbation phase and rises sharply (~10-30×) at the
perturbation → reactive-pattern transition, which is exactly what the trigger
keys on.

### Setup through jarvis

The `adios2_gray_scott` package materializes the gated XML
(`config/hermes_trigger.xml`) when `trigger=true`:

```bash
# The pkg name contains dots, so use the full global ID
jarvis pkg configure coeus-gray-scott.jarvis_coeus.adios2_gray_scott \
    engine=hermes_derived trigger=true \
    F=0.08 k=0.03 dt=1 plotgap=50 steps=1000 trigger_baseline_ratio=10
jarvis ppl kill && jarvis ppl run          # Step A as usual
```

**Always verify with `jarvis ppl print` before `jarvis ppl run`.** Older
jarvis-cd silently dropped any configure arg whose value equals the menu
default (e.g. `trigger=false`, `plotgap=10`) because it detected "explicit"
args by comparing values against defaults; fixed 2026-07-10 in
`jarvis-cd/jarvis_cd/core/pipeline.py` (explicit args are now taken from
the raw command-line tokens). On an unfixed jarvis the symptom is a run
that keeps stale values for exactly the knobs you "changed".

then Step B with `--num-steps 3`. Key knobs (full table in the package
README, `test/jarvis/.../adios2_gray_scott/README.md`):

| Config | Default | Meaning |
|---|---|---|
| `trigger` | `false` | Enable the variance trigger + gated SST |
| `trigger_variable` | auto | `derive/VarV` on `hermes_derived`, raw `V` on `hermes` |
| `trigger_threshold` | `0.05` | Absolute variance threshold (0 disables) |
| `trigger_baseline_ratio` | `0` | Fire at ratio × first-step baseline (0 disables) |
| `trigger_inspect_steps` | `3` | Output steps shipped per fire |
| `trigger_log_file` | `shared_dir/trigger_log.jsonl` | Fire-event JSONL |

**Choosing the fire condition — absolute thresholds are NOT L-portable.**
The gray-scott seed is a fixed 12³ cube regardless of `L`, so variance scales
with the seeded fraction of the domain (~1/L³): baseline ≈ 0.004 at L=64 but
≈ 8.8e-06 at L=512. The absolute `trigger_threshold=0.05` is calibrated for
L≈64-256 in the F=0.08/k=0.03 regime; at larger L (or in the slow
F=0.01/k=0.05 regime) it never fires. `trigger_baseline_ratio` is
L-independent (it measures pattern growth relative to the run's own
baseline) — prefer it when changing scale or regime.

### Expected output (verified 2026-07-09)

Writer log, at engine init and then on the firing output step:

```
Trigger: variance(derive/VarV) sum_var=derive/AddV threshold=0.05 baseline_ratio=10 inspect_steps=3 refire=false
...
Trigger FIRED at step 8: variance(derive/VarV) = 9.4e-05 (threshold 0.05, baseline 8.77e-06, ratio 10), streaming 3 step(s)
SST flagged step 8 shipped in 5429599 us (fired_now=true, window_left=3)
SST flagged step 9 shipped in 5604718 us (fired_now=false, window_left=2)
SST flagged step 10 shipped in 4941609 us (fired_now=false, window_left=1)
```

No `SST in-transit` lines appear for untriggered steps. Verified reference
points, 64 ranks / 4 nodes, F=0.08 k=0.03 dt=1 plotgap=50:

| L | Criterion | Fires at | Values |
|---|---|---|---|
| 64 | threshold 0.05 | output 9 of 20 | 0.0617 vs baseline 0.0044 |
| 256 | baseline_ratio 10 | output 8 of 20 | 5.01e-04 = 10.7× baseline 4.67e-05 (32 ranks / 2 nodes, 2026-07-11) |
| 512 | baseline_ratio 10 | output 8 of 20 | 9.40e-05 = 10.7× baseline 8.77e-06 |

At L=512 each ~2 GB flagged step ships in ~5 s and the single-process
pvbatch consumer renders each 512³ frame in ~60-75 s. Logs of record:
`logs/gs_L512_trigger_{writer,sst_consumer}.log`,
`logs/gs_L512_trigger_log.jsonl`. If the derived blob were missing the
engine would log `Trigger: derived block variance ... unavailable` and
contribute an empty block — zero such warnings in a healthy run.

### Worked example: 32 ranks / 2 producer nodes + 1 reader node (2026-07-11)

Complete recipe for the L=256 reference row above, run on the hostfile
`ares-comp-18/20/22/23/25/26` (both hostfiles listing all six nodes; the
producer takes the first two, the reader ran on ares-comp-22).

```bash
# 0. Both hostfiles list all nodes (see the two-hostfile table in Section 2):
#    /mnt/common/hxu40/jarvis-pipelines/coeus-gray-scott/hostfile
#    /home/hxu40/server_list/server.list

# 1. Configure — 32 ranks @ 16/node -> first two hostfile nodes
spack load iowarp@main adios2-coeus@vigil
jarvis pkg configure coeus-gray-scott.jarvis_coeus.adios2_gray_scott \
    nprocs=32 ppn=16 L=256 engine=hermes_derived trigger=true \
    F=0.08 k=0.03 dt=1 plotgap=50 steps=1000 \
    trigger_baseline_ratio=10 trigger_inspect_steps=3
jarvis ppl print | grep -E "nprocs|trigger|engine"    # VERIFY before running

# 2. Clean stale contact files, then launch (writer blocks at rendezvous)
rm -f /mnt/common/hxu40/coeus/iowarp/coeus-adapter/gs.bp.sst ~/gs.bp.sst
jarvis ppl kill && jarvis ppl run

# 3. On a NON-producer node, once gs.bp.sst appears, start the reader
#    (--num-steps must equal trigger_inspect_steps)
spack load paraview
cd coeus-adapter/test/real_apps/gray-scott
pvbatch catalyst/gs-pipeline.py -j $PWD/catalyst/gs-fides.json \
    -b /mnt/common/hxu40/coeus/iowarp/coeus-adapter/gs.bp \
    --staging --num-steps 3
```

Observed result: outputs 1-7 ship nothing (reader blocks inside SST);
`Trigger FIRED at step 8: variance(derive/VarV) = 5.01e-04` (10.7× baseline
4.67e-05); flagged steps 8/9/10 ship in 0.4-1.2 s each (~130 MB/field at
L=256); the reader renders `output-0000{0,1,2}.png` and exits by itself;
the writer drains outputs 11-20 untriggered, logs `DoClose` for all ranks,
and removes `gs.bp.sst` on exit. The fire event is appended to
`trigger_log.jsonl` in the package shared dir.

Note on the rendered isosurfaces: faint grid lines on the blob are the
contour crossing writer-block boundaries (the Fides multiblock output has
no ghost cells) — a cosmetic artifact, not data corruption. Add a
ghost-cell generator or MergeBlocks before the Contour in `gs-pipeline.py`
if clean surfaces are needed.

## 4. AI-agent SST consumer: ParaView + MCP (trigger → render → reason)

Instead of the fixed `pvbatch` reader (Step B), the SST consumer can be an
**LLM agent** that steers ParaView through MCP tools: it pauses/advances the
stream, creates isosurfaces/slices on the live data, takes screenshots, and
describes what it sees. Everything lives in `test/insitu_agent/`. Verified
2026-07-10 with `claude-haiku-4-5` (25 tool calls, 8 screenshots, ~$0.11,
35 s) and combined with the trigger the same week.

```
gray-scott (jarvis, SST writer) ──gs.bp──► insitu_streaming.py (pvpython bridge)
                                                │ renders each step + saves PNG
                                            pvserver --multi-clients :11112
                                                ▲
                                     insitu_mcp_server.py (own render view)
                                                ▲ stdio MCP
                                        insitu_agent.py (LLM tool loop)
```

All consumer processes run on a **non-producer node** (they share the node
fine). Order: pvserver → jarvis pipeline (Step A) → bridge → agent.

```bash
# 1. pvserver (same paraview spack install as pvbatch; 11111 is often busy)
spack load paraview
pvserver --multi-clients --server-port=11112 &

# 2. Step A as usual (Section 2/3). Writer blocks at rendezvous until the
#    bridge's Fides reader connects.

# 3. Streaming bridge — replaces the pvbatch reader. MUST run under pvpython;
#    -b MUST be the absolute path next to gs.bp.sst; --paused gives the agent
#    step control; --max-steps caps runaway consumption.
cd coeus-adapter/test/insitu_agent
pvpython insitu_streaming.py -j $PWD/gs-fides.json \
    -b /mnt/common/hxu40/coeus/iowarp/coeus-adapter/gs.bp \
    --staging --server localhost --port 11112 --paused \
    --screenshot-file /tmp/bridge_view.png --max-steps 60 &

# 4. The agent. It spawns insitu_mcp_server.py itself (stdio MCP) using its
#    own interpreter, so that python must import paraview: use the spack
#    python 3.12 (system python3 has an OpenSSL mismatch) with the paraview
#    site-packages + ~/software/paraview_mcp on PYTHONPATH.
export ANTHROPIC_API_KEY=sk-ant-...
PV=$(spack location -i paraview)
SPY=$(spack location -i python)/bin/python3
env PYTHONPATH="$PV/lib/python3.12/site-packages:$HOME/software/paraview_mcp" \
    LD_LIBRARY_PATH="$PV/lib" \
$SPY insitu_agent.py --provider anthropic --model claude-haiku-4-5 \
    --server-host localhost --server-port 11112 \
    --screenshot-file /tmp/bridge_view.png \
    --results-dir ./agent_results \
    --max-iterations 15 --max-wall-seconds 240 \
    --prompt "Advance one step, take a screenshot and describe the pattern. \
Then create an isosurface of V at a sensible value and describe the 3D structure."
```

Per run the agent saves `token_usage.json` (tokens, cost, tool/screenshot
counts) and every screenshot under `--results-dir`.

**With the trigger enabled** (Section 3) the semantics compose naturally:
the bridge receives nothing until the trigger fires, so the agent's
`advance_step` simply blocks until the first flagged step arrives, then the
agent inspects the `trigger_inspect_steps` shipped steps one by one. While
gated, `QueueFullPolicy=Block` holds flagged steps until the agent consumes
them — nothing is dropped while the LLM "thinks". Give the agent a prompt
budget that matches the window (e.g. 3 advance/screenshot rounds for
`trigger_inspect_steps=3`).

Gotchas (each cost a debugging session — details in
`test/insitu_agent/README.md`):

- The bridge must run under **pvpython**, the MCP server/agent under the
  **spack python** — never swap them (pvpython's VTK stdout wrapper breaks
  stdio MCP; plain python can't import paraview without the PYTHONPATH above).
- **Do not share the bridge's render view.** The MCP server creates its own
  view at connect (`_isolate_mcp_view`, added 2026-07-10). Before that fix,
  any `create_isosurface` broke every subsequent bridge render — the
  "empty screenshot" bug (6.5 KB PNGs of background + axes). If empty
  screenshots return, suspect another ParaView client Show()ing filters into
  the bridge's view; `INSITU_DEBUG_VIEW=1` on the bridge dumps per-render
  state. `get_screenshot` serves the bridge PNG for the live volume and
  renders the MCP view when the agent has created filters.
- Model ids: use the official Anthropic API — set `ANTHROPIC_API_KEY` to
  your `sk-ant-...` key and pass e.g. `claude-haiku-4-5` / `claude-sonnet-4-5`.

**Alternative — Claude Code as the agent**: register the MCP server in
`.mcp.json` and Claude Code drives the same tools interactively (no
`insitu_agent.py` needed):

```json
{ "mcpServers": { "InSitu-ParaView": {
    "command": "/path/to/spack/python3",
    "args": ["/abs/path/test/insitu_agent/insitu_mcp_server.py",
             "--server", "localhost", "--port", "11112",
             "--status-file", "/abs/path/test/insitu_agent/streaming_status.json",
             "--screenshot-file", "/tmp/bridge_view.png"],
    "env": {"PYTHONPATH": "<paraview site-packages>:<paraview_mcp dir>",
            "LD_LIBRARY_PATH": "<paraview lib>"} } } }
```

## 5. Shutdown behavior and cleanup

- **The reader hangs at end-of-run by design.** The writer skips SST
  `Close()` (it would block), waits 4 s, destroys the engine, and removes
  `gs.bp.sst`. The reader never receives EndOfStream and sits at
  `Waiting for step N...` — kill it, or use the contact-file watchdog
  pattern from `test/real_apps/gray-scott/run-sst.sh` (kills the reader when
  `gs.bp.sst` disappears). The same applies to the agent-mode bridge
  (Section 4): after the writer exits, `advance_step` can never complete —
  kill the bridge (and pvserver if done); `--max-steps` bounds it too.
- **Remove stale `gs.bp.sst` files before a rerun** (repo root and `$HOME`);
  a leftover contact file from a dead writer will confuse the next reader.
- `jarvis ppl kill` stops the runtime daemon and any leftover ranks.

## 6. Troubleshooting a "hang"

Where the writer log stops tells you the phase:

| Last log line | Meaning |
|---|---|
| `requested more processes than the ppr ...` (exits in seconds) | mpirun node pool too small — the **jarvis global hostfile** (`server_list/server.list`) doesn't list all nodes; see the two-hostfile note above |
| stuck before `consensus rank` lines | Chimaera runtime/client connect issue — is the runtime up? (`clio_run compose list` on the writer node answers instantly if healthy) |
| stops right after `varFile: ...` | **Waiting for the SST reader** (the case above) — start Step B |
| steps flow but no CTE data | check tags with `cte_search ".*" ".*"` on the writer node |
| trigger mode: steps flow but the reader never receives anything | the trigger never fired — no `Trigger FIRED` line and an empty `trigger_log.jsonl`. Wrong regime (F=0.01/k=0.05 barely moves variance) or an absolute threshold at the wrong scale (see the L-portability note in Section 3); switch to `trigger_baseline_ratio` |

Useful probes (writer node, `spack load iowarp@main`):

```bash
clio_run compose list        # active pools: admin, clio_cte_core, ram tier, rankConsensus, db_operation
cte_search ".*" ".*"         # list CTE tags/blobs (step_{N}_rank{R} per step)
```

Note: gdb attach is blocked on Ares (`yama/ptrace_scope=1`); inspect
`/proc/<pid>/task/*/wchan`, `/proc/<pid>/fd`, and CPU state instead. All
ranks spinning at ~100% CPU is what a blocked clio task `Wait()` or an SST
rendezvous wait looks like.
