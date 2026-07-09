# Building COEUS-Adapter and Running Gray-Scott (clio-core backbone)

Verified end-to-end on Ares, 2026-07-09 (writer: `ares-comp-11`, reader:
`ares-comp-14`).

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

### Expected output

Writer (`jarvis ppl run` stdout): consensus-rank assignment for all 16
ranks, `Catalyst SST stream: gs.bp (multi-node)`, then per output step

```
Simulation at step 10 writing output step 1
[Coeus engine] SST in-transit step 1: Put time (us): ..., EndStep time (us): ...
```

through step 150 (output 15), a `ckpt.bp` checkpoint at step 70, then
`DoClose` messages and a clean jarvis exit.

## 3. Shutdown behavior and cleanup

- **The reader hangs at end-of-run by design.** The writer skips SST
  `Close()` (it would block), waits 4 s, destroys the engine, and removes
  `gs.bp.sst`. The reader never receives EndOfStream and sits at
  `Waiting for step N...` — kill it, or use the contact-file watchdog
  pattern from `test/real_apps/gray-scott/run-sst.sh` (kills the reader when
  `gs.bp.sst` disappears).
- **Remove stale `gs.bp.sst` files before a rerun** (repo root and `$HOME`);
  a leftover contact file from a dead writer will confuse the next reader.
- `jarvis ppl kill` stops the runtime daemon and any leftover ranks.

## 4. Troubleshooting a "hang"

Where the writer log stops tells you the phase:

| Last log line | Meaning |
|---|---|
| `requested more processes than the ppr ...` (exits in seconds) | mpirun node pool too small — the **jarvis global hostfile** (`server_list/server.list`) doesn't list all nodes; see the two-hostfile note above |
| stuck before `consensus rank` lines | Chimaera runtime/client connect issue — is the runtime up? (`clio_run compose list` on the writer node answers instantly if healthy) |
| stops right after `varFile: ...` | **Waiting for the SST reader** (the case above) — start Step B |
| steps flow but no CTE data | check tags with `cte_search ".*" ".*"` on the writer node |

Useful probes (writer node, `spack load iowarp@main`):

```bash
clio_run compose list        # active pools: admin, clio_cte_core, ram tier, rankConsensus, db_operation
cte_search ".*" ".*"         # list CTE tags/blobs (step_{N}_rank{R} per step)
```

Note: gdb attach is blocked on Ares (`yama/ptrace_scope=1`); inspect
`/proc/<pid>/task/*/wchan`, `/proc/<pid>/fd`, and CPU state instead. All
ranks spinning at ~100% CPU is what a blocked clio task `Wait()` or an SST
rendezvous wait looks like.
