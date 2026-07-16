# jarvis_coeus.lbm_cfd

Runs the 2D LBM-CFD example
(`test/real_apps/ascent-trame/examples/lbm-cfd`) through the COEUS hermes plugin
engine as the **instability-onset** case of the Vigil
[Trigger-Render-Reason pipeline](../../../../../docs/TRIGGER_RENDER_REASON_PIPELINE.md).

Pipeline: `clio_runtime` → `clio_cte` → `lbm_cfd`. The writer streams `vorticity`
and declares `derive/VarVort = variance(vorticity)`; the engine pools it into the
exact global variance every output step and fires when the D2Q9 scheme goes
unstable (stable variance ~1e-5, unstable ~1e8).

## Prerequisites

`coeus-adapter/build/bin` on `PATH` + `LD_LIBRARY_PATH` (supplies `lbmcfd`'s
`libhermes_engine.so`), and the app built against `adios2-coeus@vigil`:

```bash
spack load iowarp@main adios2-coeus@vigil openmpi
export PATH=<coeus>/build/bin:$PATH
export LD_LIBRARY_PATH=<coeus>/build/bin:$LD_LIBRARY_PATH
export ADIOS2_PLUGIN_PATH=<coeus>/build/bin
# build the app: (in the lbm-cfd dir) export ADIOS2_DIR=$(spack location -i adios2-coeus@vigil); make
```

## Key knobs (`jarvis pkg configure jarvis_coeus.lbm_cfd key=value`)

| knob | meaning | default |
|---|---|---|
| `lbm_bin` | absolute path to the ADIOS2-enabled `lbmcfd` binary | built location |
| `script_location` | NFS-shared run dir (cwd; configs + flags land here) | required |
| `engine` | `hermes` or `bp5` | `hermes` |
| `nprocs` / `ppn` | MPI ranks / per node | 4 / 4 |
| `steps` / `force_unstable` | LBM steps; deterministic blow-up case | 20000 / false |
| `trigger` + `trigger_variable` / `trigger_sum_variable` | `variance` trigger on `derive/VarVort` (+ `derive/AddVort`) | derive/VarVort |
| `trigger_threshold` | absolute variance threshold (stable ~1e-5, unstable ~1e8) | 1.0 |
| `catalyst_stream` | non-empty ⇒ trigger-gated SST render stream | `''` (log-only) |
| `sst_data_transport` / `sst_queue_full_policy` | SST transport / queue policy | WAN / engine default |

## Ready-to-run pipelines

- **`pipelines/lbm-cfd.yaml`** — log-only fire demo (`force_unstable`,
  `variance(derive/VarVort)` threshold 1.0). Fire events →
  `<script_location>/lbm_trigger_log.jsonl`.
- **`pipelines/lbm-cfd-sst.yaml`** — trigger-gated SST render. The writer blocks
  until a reader connects; start the reader after `jarvis ppl run`:

```bash
jarvis ppl load yaml pipelines/lbm-cfd-sst.yaml
jarvis ppl kill && jarvis ppl run          # writer waits for the reader
# once <script_location>/lbm_sst.bp.sst appears, in an ISOLATED adios2-only env
# (iowarp pollutes numpy — see the memory note), run the reader:
python3 <lbm-cfd>/consumer/lbm_sst_reader.py \
    --stream <script_location>/lbm_sst.bp --engine SST --max-steps 3
```

The reader receives only the flagged window (vorticity + `vigil/trigger_*`).

## Reason step (rescue / stop)

Run `lbmcfd` with `--agent-rescue` to disable the automatic self-heal and let an
agent decide. The `consumer/lbm_insitu_mcp_server.py` MCP server exposes
`inspect_latest_frame` (reads the reader's `--status-file`),
`fire_rescue_simulation` (writes `<out>.rescue` → the sim reverts to its last
checkpoint and doubles the timesteps) and `fire_stop_simulation` (writes
`<out>.stop` → halt). The sim polls both flags each output step.

## Gotchas

- Single-node run: the app's `mpirun` uses the **global** `server.list`; point it
  at the local node for a local run.
- `jarvis pkg configure force_unstable=false` is dropped (`false` == the menu
  default) — flip it via a dedicated YAML instead.
- The SST reader needs adios2's python; don't load `iowarp@main` in its shell.
