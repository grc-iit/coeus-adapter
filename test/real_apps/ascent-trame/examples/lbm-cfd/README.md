# LBM-CFD

Lattice-Boltzmann Method 2D Computational Fluid Dynamics Simulation

Code is parallelized using MPI

---

## ADIOS2 SST streaming (Trigger-Render-Reason)

This 2D case can stream its **vorticity** field over ADIOS2 — **SST** for live
streaming to an external consumer, or **BP5** to a file — as the first step
toward the COEUS
[Trigger-Render-Reason pipeline](../../../../../docs/TRIGGER_RENDER_REASON_PIPELINE.md).
Only vorticity is written as a *raw* field; the instability trigger statistic is
produced **in situ** by an ADIOS2 derived-variable operator
(`derive/VarVort = variance(vorticity)`), the same pattern gray-scott uses
(`derive/VarV`). As the D2Q9 scheme goes unstable, `variance(vorticity)` spikes
by many orders of magnitude — a cheap, collective instability detector.

### Build with ADIOS2

Needs an ADIOS2 with SST + derived variables (the COEUS `adios2-coeus@vigil`
spack install provides both):

```bash
source <spack>/share/spack/setup-env.sh
spack load adios2-coeus@vigil openmpi
export ADIOS2_DIR=$(spack location -i adios2-coeus@vigil)
make            # -DADIOS2_ENABLED is added automatically when ADIOS2_DIR is set
```

`ADIOS2_DIR` and `ASCENT_DIR` are independent — set either, both, or neither.

### Run — file (BP5), self-contained

```bash
./bin/lbmcfd --adios2 --adios-config adios2-bp5.xml --output-bp lbmcfd.bp
bpls -l lbmcfd.bp
#   double  vorticity      N*{240, 600}   raw field (global, row-major y,x)
#   double  derive/VarVort N*{1}          variance(vorticity)  <- trigger signal
#   double  derive/AddVort N*{240}        block sums (pool a global variance)
#   int32_t step / stable  N*scalar       time / stability flag
```

### Run — live stream (SST) + standalone consumer

The writer's `Open()` blocks until a reader connects (`RendezvousReaderCount=1`
in `adios2.xml`), so no output steps are lost. In two terminals (same directory):

```bash
# terminal 1 — consumer (zero COEUS dependency)
python3 consumer/lbm_sst_reader.py --stream lbmcfd.bp --config adios2.xml
#   --png-dir frames/   also renders one vorticity image per step (needs matplotlib)

# terminal 2 — simulation (writer)
mpiexec -np 4 ./bin/lbmcfd --adios2 --adios-config adios2.xml --output-bp lbmcfd.bp
```

Add `--force-unstable` to the writer for a fast, deterministic run whose
vorticity blows up — the consumer reports the `stable` flag flipping `1 -> 0`
and `variance(vorticity)` exploding, e.g.:

```
[consumer] recv #1 step=100 stable=1 shape=(240, 600) vort[min=-404.2 max=405.6]
[consumer] recv #2 step=200 stable=0 shape=(240, 600) vort[min=-3.91e+04 max=3.911e+04]
[consumer] recv #3 step=300 stable=0 shape=(240, 600) vort[min=-1.458e+06 max=1.458e+06]
```

### Reason step — agent-driven rescue / stop

Run with `--agent-rescue` to disable the simulation's automatic self-heal and let
an AI agent decide instead. The sim then polls two verdict flags in its run
directory at every output step:

| Flag | Effect |
|---|---|
| `<out_file>.rescue` | revert to the last checkpoint and **double the timesteps** (halving the lattice speed) to restabilise the D2Q9 scheme |
| `<out_file>.stop` | halt the run early |

`consumer/lbm_insitu_mcp_server.py` is the agent's interface (no ParaView
needed). Pair it with the reader's `--status-file`:

```bash
# reader: consume the gated window and publish each frame's stats
python3 consumer/lbm_sst_reader.py --stream <run_dir>/lbm_sst.bp --engine SST \
    --status-file <run_dir>/lbm_streaming_status.json

# MCP server: the agent inspects frames and issues the verdict
python3 consumer/lbm_insitu_mcp_server.py \
    --status-file <run_dir>/lbm_streaming_status.json \
    --rescue-flag <run_dir>/lbmcfd.bp.rescue \
    --stop-flag   <run_dir>/lbmcfd.bp.stop
```

Tools: `inspect_latest_frame()` (vorticity range, non-finite cells, trigger
stat — flags `DIVERGING`), `fire_rescue_simulation(reason)`,
`fire_stop_simulation(reason)`. `--selftest` exercises them without an agent.

Note the rescue only helps if the doubled timestep count lands in the stable
regime — for this 600x240 case, ~8000 steps is still unstable while ~12000 is
stable, so one rescue from 6000 recovers. The agent can rescue repeatedly
(re-inspect, fire again) if one doubling isn't enough.

### Command-line flags (added for ADIOS2 / the agent)

| Flag | Meaning | Default |
|---|---|---|
| `--adios2` | enable ADIOS2 output | off |
| `--output-bp <name>` | stream / file name (implies `--adios2`) | `lbmcfd.bp` |
| `--adios-config <xml>` | ADIOS2 XML (engine: `adios2.xml`=SST, `adios2-bp5.xml`=BP5) | `adios2.xml` |
| `--no-derived` | skip the `variance`/`add` derived variables | derived on |
| `--agent-rescue` | disable the automatic rescue; poll the agent's `.rescue` / `.stop` verdict flags | off |

Files: `include/adios2_writer.hpp` (writer), `adios2.xml` / `adios2-bp5.xml`
(engine config), `consumer/lbm_sst_reader.py` (consumer),
`consumer/lbm-fides.json` (Fides data model for ParaView).

---


### Build LBM-CFD application
With Ascent support:
```
env ASCENT_DIR=<ascent_install_dir>/install/ascent-checkout make
```

Without Ascent support:
```
make
```


### Run Trame server
Make sure that the Python virtual environment created for Ascent install is activated

```
python trame_app.py --host 0.0.0.0 --port <port> --server --timeout 0
```


### Run LBM-CFD application
Make sure that the Python virtual environment created for Ascent install is activated

Ensure Trame server is up and running, and you have opened the web page to the Trame application

Then run the following (note: you may need to specify different versions for Python and Conduit - check your install)
```
PYTHON_SITE_PKG="<python_virtual_env_path>/lib/python3.12/site-packages"
ASCENT_DIR="<ascent_install_dir>/install"
export PYTHONPATH=$PYTHONPATH:PYTHON_SITE_PKG:$ASCENT_DIR/ascent-checkout/python-modules/:$ASCENT_DIR/conduit-v0.9.2/python-modules/

mpiexec -np <num_procs> ./bin/lbmcfd
```

