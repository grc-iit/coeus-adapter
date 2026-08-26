[← COEUS-Adapter README](../README.md) · [Trigger-Render-Reason](TRIGGER_RENDER_REASON_PIPELINE.md) · [Build & Run (Gray-Scott)](BUILD_AND_RUN_GRAY_SCOTT.md) · [Delta AD](ARTIFACT_DESCRIPTION_DELTA.md)

# Running Vigil on Chameleon Cloud

This document records the platform-specific deviations needed to reproduce the
Gray-Scott **collapse-warning** case (trigger → render → reason) on a
Chameleon Cloud allocation. It is a **delta, not a walkthrough**: apart from
the items below,
[BUILD_AND_RUN_GRAY_SCOTT.md](BUILD_AND_RUN_GRAY_SCOTT.md) §7 applies
unchanged, and its trigger semantics, tuned gate levels, and expected agent
behaviour are the reference.

Read §7 of that document first. Everything here assumes it.

## 1. Platform

| Item | Value |
|---|---|
| Allocation | 2 nodes (1 producer, 1 consumer) |
| CPU | 2× Intel Xeon Gold 6126 per node |
| OS image | Ubuntu 24.04 |
| Render path | **Software GL (OSMesa)** - no render GPU, the same path used on Delta CPU nodes |
| Case | Gray-Scott collapse-warning, `gray-scott-warn-collapse.yaml` |

The absence of a render GPU is the important property: it puts Chameleon on
the same headless OSMesa path as Delta, so the ParaView requirements of
[ARTIFACT_DESCRIPTION_DELTA.md](ARTIFACT_DESCRIPTION_DELTA.md) §3 carry over
in full, plus the version pins in §3 below.

## 2. The deviations, in one table

| # | Deviation | Section |
|---|---|---|
| 1 | **Pin Python and HDF5 when rebuilding ParaView** - the stock spec does not concretize on a fresh Ubuntu 24.04 Spack | [§3](#3-paraview-the-pinned-rebuild) |
| 2 | Build the engine with `-DCOEUS_ENABLE_CATALYST=ON` - with it off, triggers still evaluate but **nothing streams**, which presents as a hang rather than an error | [§4](#4-build-the-engine-with-coeus_enable_catalyston) |
| 3 | Override the network interface name - the Ares/Delta names (`eno1`, `hsn0`) do not exist here | [§5](#5-network-interface) |
| 4 | Put the SST contact file **and** the agent stop flag on the shared filesystem, not node-local storage | [§6](#6-shared-filesystem-placement) |

Deviation 1 is where the time goes. The rest are one-line changes.

## 3. ParaView: the pinned rebuild

### 3.1 Why the stock spec fails here

The Delta AD installs ParaView with:

```bash
spack install paraview@5.13.3 +adios2+fides+mpi+python~qt ^mesa~glx+osmesa
```

On a Chameleon Ubuntu 24.04 image with a fresh Spack tree, that spec has no
pinned Python or HDF5, so the concretizer is free to pick current defaults for
both. Both defaults are wrong for this build:

- **Python.** ParaView 5.13's VTK Python wrapping does not support the Python
  version a current Spack concretizes to by default. The build fails during
  wrapping rather than at concretization, so the failure arrives late, after
  the expensive part.
- **HDF5.** `paraview +adios2+fides+mpi` and `adios2-coeus@vigil` both pull
  HDF5. If they land on different HDF5 versions or different variants, the
  concretizer either refuses the spec outright or produces a ParaView whose
  embedded ADIOS2 cannot open the writer's SST stream at runtime.

The engine side already fixes its half of this: `adios2-coeus`'s recipe
constrains HDF5 by variant (`hdf5~mpi` when `~mpi`, `hdf5+mpi` when `+mpi`,
see `CI/coeus/packages/adios2-coeus/package.py`). ParaView must be pinned to
match, by hand.

### 3.2 The spec to use

```bash
spack install paraview@5.13.3 \
    +adios2+fides+mpi+python~qt \
    ^python@3.12 \
    ^hdf5@1.14+mpi \
    ^mesa~glx+osmesa
```

Each constraint is load-bearing:

| Constraint | Why |
|---|---|
| `@5.13.3` | The reader scripts target the ParaView 5.13 Fides API. Newer ParaView moves it. |
| `+adios2+fides` | The SST reader path (`insitu_streaming.py`, `gs-pipeline.py`) goes through Fides over ADIOS2. |
| `+mpi` | Must match the writer's MPI so the embedded ADIOS2 speaks the same SST. |
| `+python~qt` | `pvpython` is the bridge interpreter; Qt is dead weight on a headless node. |
| `^python@3.12` | The version ParaView 5.13 wraps cleanly, and the one the rest of the artifact already assumes. |
| `^hdf5@1.14+mpi` | Matches what `adios2-coeus@vigil` concretizes to. `+mpi` must agree on both sides. |
| `^mesa~glx+osmesa` | Provides `libOSMesa`. Without it there is no software GL and rendering needs an X server that does not exist. |

### 3.3 On `^python@3.12`

3.12 is not an arbitrary choice; it is the version the rest of the artifact is
already written against. The agent invocation in
[BUILD_AND_RUN_GRAY_SCOTT.md](BUILD_AND_RUN_GRAY_SCOTT.md) §4 and §7.4 puts
ParaView's site-packages on `PYTHONPATH` by literal path:

```bash
PV=$(spack location -i paraview)
env PYTHONPATH="$PV/lib/python3.12/site-packages:$HOME/software/paraview_mcp" ...
```

If ParaView is built against a different Python, that path does not exist and
the MCP server fails to `import paraview` - which surfaces as the agent
starting and immediately failing its first tool call, not as a build error.
Pinning 3.12 keeps every documented command line correct as written.

The MCP server and agent must run under the **Spack** Python 3.12 (it has
`mcp` and `anthropic`), while the bridge runs under **pvpython**. Do not swap
them; see the gotcha list in §4 of the Gray-Scott document.

### 3.4 On `^hdf5@1.14+mpi`

The two consumers of HDF5 in this stack are ParaView's embedded ADIOS2 and the
writer's `adios2-coeus@vigil`. SST is a rendezvous protocol between the two,
so a version or variant split does not fail cleanly - it fails at connect time,
after the writer is already blocked waiting for a reader.

Pin both sides to the same HDF5 and confirm it before running:

```bash
spack spec -I paraview@5.13.3 +adios2+fides+mpi+python~qt \
    ^python@3.12 ^hdf5@1.14+mpi ^mesa~glx+osmesa | grep -E "hdf5|python|adios2|mesa"
spack spec -I adios2-coeus@vigil +pic +shared | grep hdf5
```

The `hdf5` lines should agree in version and in the `+mpi` variant. If they do
not, fix it at concretization time; there is no runtime workaround.

### 3.5 Install the Python packages into that interpreter

The agent and MCP server import `mcp` and `anthropic` from the Spack Python
3.12 that pvpython uses:

```bash
python3 -m pip install --user "mcp[cli]" httpx anthropic openai
```

### 3.6 Verify, and load by hash

Two `paraview@5.13.3` installs can coexist and only the `~x` / `+osmesa` one
renders headlessly, so record the hash and load by it:

```bash
spack find -lv paraview      # note the hash of the '~x ... ^...osmesa' spec
spack load /<hash>
```

Confirm the build before involving the simulation at all:

```bash
spack find -v /<hash> | grep -E "~x|osmesa"     # the spec really is the headless one
pvpython -c "import paraview; print(paraview.__file__)"
pvbatch -c "from paraview.simple import *; Show(Sphere()); Render(); SaveScreenshot('/tmp/gl_check.png')"
```

A non-empty `/tmp/gl_check.png` means software GL is working. If instead it
aborts with `bad X server connection`, the wrong ParaView is loaded - go back
to the hash.

## 4. Build the engine with `COEUS_ENABLE_CATALYST=ON`

`COEUS_ENABLE_CATALYST` defaults to `OFF`
(`CMakeLists.txt:42`), and `src/CMakeLists.txt` defines `COEUS_HAVE_CATALYST`
only when Catalyst is found **or** the option is forced on. On Chameleon the
find does not succeed, so the option must be explicit:

```bash
cd coeus-adapter && mkdir -p build && cd build
cmake .. -DCOEUS_ENABLE_CATALYST=ON
make -j8
```

**This is the deviation most likely to cost a debugging session.** With
Catalyst compiled out, the engine still evaluates the variance trigger
correctly and still writes `trigger_log.jsonl`, but there is no Catalyst SST
stream for flagged steps to be re-Put into. Nothing is emitted, the reader
never receives a step, and the run presents as a **hang** with no error on
either side.

Distinguishing it from the other hangs in §6 of the Gray-Scott document: here
the writer log shows the trigger arming and warning normally while the reader
sits at `Waiting for step ...` forever. Check the configure output for

```
Catalyst not found and COEUS_ENABLE_CATALYST=OFF: building without in-situ integration
```

which is the symptom printed at build time, long before the hang.

## 5. Network interface

The reference documents pin OpenMPI to interface names that do not exist on
Chameleon: `eno1` on Ares, `hsn0` (Slingshot) on Delta. Find the actual
interface carrying the node-to-node network and substitute it:

```bash
ip -br addr        # identify the interface on the inter-node subnet
IFACE=<name from above>
export OMPI_MCA_pml=ob1
export OMPI_MCA_btl=tcp,self
export OMPI_MCA_osc='^ucx'
export OMPI_MCA_btl_tcp_if_include=$IFACE
export OMPI_MCA_oob_tcp_if_include=$IFACE
```

The failure mode for a wrong interface is
`PRTE has lost communication with a remote daemon`, as documented for Ares.

Note that no `srun`/PMIx shim appears in this deviation list: the ordinary
`mpirun` launch path of the Gray-Scott walkthrough applies here, and the
Delta-specific `CI/Delta/jarvis-ssh-local-shim.sh` machinery is not used. If
inter-node ssh turns out to be denied on your allocation, fall back to the
mechanism described in [ARTIFACT_DESCRIPTION_DELTA.md](ARTIFACT_DESCRIPTION_DELTA.md) §7.

## 6. Shared-filesystem placement

Two files are exchanged between the producer and the consumer out of band, and
both must live on the **shared** filesystem. Node-local storage silently
breaks each in a different way.

**SST contact file.** The writer creates `gs.bp.sst` in the simulation's
working directory when it reaches rendezvous; the reader's `-b` argument must
be the absolute path next to it. If that directory is node-local, the consumer
node never sees the file, so the reader never connects and the writer waits at
rendezvous forever.

**Agent stop flag.** `fire_stop_simulation` writes `<out_file>.stop` on the
consumer node; the Gray-Scott loop polls for it on the producer after each
output and breaks collectively (`MPI_LOR`). If the flag lands on node-local
storage, the agent fires its verdict, reports success, and the writer runs to
completion anyway - the early stop simply does not happen, with nothing in
either log calling it out.

Point both at the shared path, and pass the stop flag explicitly:

```bash
pvpython insitu_streaming.py -j $PWD/gs-fides.json \
    -b <shared-path>/gs.bp \
    --staging --server localhost --port 11112 --paused \
    --screenshot-file /tmp/bridge_view.png --max-steps 8 &

$SPY insitu_agent.py ... --stop-flag <shared-path>/<out_file>.stop
```

Clear stale contact files before every rerun (repo root and `$HOME`); a
leftover `gs.bp.sst` from a dead writer will confuse the next reader.

## 7. Running the case

With §3-§6 applied, follow
[BUILD_AND_RUN_GRAY_SCOTT.md](BUILD_AND_RUN_GRAY_SCOTT.md) §7.3-§7.4 as
written. In outline, on the two nodes:

```bash
# Consumer node
spack load /<paraview-hash>
pvserver --multi-clients --server-port=11112 &

# Producer node
spack load iowarp@main adios2-coeus@vigil
jarvis ppl load yaml test/jarvis/jarvis_coeus/pipelines/vigil/gray-scott/gray-scott-warn-collapse.yaml
jarvis ppl print | grep -E "trigger|collapse|engine|L:|F:|k:"   # VERIFY
jarvis ppl kill && jarvis ppl run

# Consumer node, once gs.bp.sst appears on the shared filesystem
#   bridge, then agent - see §7.4 for both command lines
```

The gate levels (`trigger_baseline_ratio=20` to arm,
`trigger_collapse_baseline_ratio=13` to warn, `trigger_inspect_steps=4`,
`plotgap=50`) are the tuned values from §7.2 and are scale-portable - they are
ratios against the run's own baseline, so they do not need re-tuning for this
platform. Do not substitute an absolute `trigger_threshold`; §3 of that
document explains why absolute thresholds are not portable.

## 8. Reported result

The Artifact Description reports, for this configuration:

| Quantity | Value |
|---|---|
| Trigger | Variance trigger fired at the documented output step |
| Stream + render | Flagged frames streamed and rendered over gated SST |
| Early stop | Agent halted the writer early, **39% of the run skipped** |
| Latency, first flagged frame → on-disk stop flag | **≈16.7 s** |
| Agent cost | **$0.08** |

On a problem this small the LLM, not the simulation, dominates latency: the
16.7 s above is essentially the agent's inspect-and-decide loop.

Note that the skipped fraction is smaller than the ~70% reported on Ares at
`L=64` in §7.5. The fraction is a function of how early the agent's verdict
lands relative to the configured `steps`, so it is expected to move with agent
latency and run length; the pass/fail criterion is that the writer halts early
at all and that the pattern-forming control run does **not** stop.

Run the `F=0.03 k=0.062` control as well. Its variance never arms, so it must
never warn and never stop. A stop there is a false positive and a real failure.

## 9. Troubleshooting

Platform-specific symptoms. For everything else, use the tables in
[BUILD_AND_RUN_GRAY_SCOTT.md](BUILD_AND_RUN_GRAY_SCOTT.md) §6 and
[ARTIFACT_DESCRIPTION_DELTA.md](ARTIFACT_DESCRIPTION_DELTA.md) §10.

| Symptom | Cause / fix |
|---|---|
| ParaView build fails during VTK Python wrapping | Python not pinned - rebuild with `^python@3.12` (§3.2) |
| Spack refuses the ParaView spec, or the reader cannot open the writer's stream | HDF5 split between ParaView and `adios2-coeus` - pin `^hdf5@1.14+mpi` on both and re-check with `spack spec` (§3.4) |
| MCP server fails to `import paraview`; agent dies on its first tool call | `PYTHONPATH` points at `lib/python3.12/site-packages` but ParaView was built against another Python (§3.3) |
| `bad X server connection` from pvserver, pvbatch, or pvpython | Wrong ParaView loaded - use the `~x` / `+osmesa` hash (§3.6) |
| Trigger warns in the writer log, reader waits forever, no error anywhere | Engine built without Catalyst - rebuild with `-DCOEUS_ENABLE_CATALYST=ON` (§4) |
| `PRTE has lost communication with a remote daemon` | Wrong interface name in `OMPI_MCA_*_if_include` (§5) |
| Writer never leaves SST rendezvous | Contact file on node-local storage, or a stale `gs.bp.sst` from a previous run (§6) |
| Agent fires and reports success, but the writer runs to completion | Stop flag on node-local storage, so the producer never sees it (§6) |
