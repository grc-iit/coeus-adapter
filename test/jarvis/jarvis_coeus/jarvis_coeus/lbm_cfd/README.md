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
| `agent_rescue` | **reason step**: turn off the sim's automatic self-heal and poll the agent's `<out_file>.rescue` / `.stop` verdict flags instead | false |

## Ready-to-run pipelines

| pipeline | stages | what it does |
|---|---|---|
| `pipelines/lbm-cfd.yaml` | trigger | log-only fire → `<script_location>/lbm_trigger_log.jsonl` |
| `pipelines/lbm-cfd-sst.yaml` | trigger + render | gated SST stream; a reader consumes the flagged window |
| `pipelines/lbm-cfd-agent.yaml` | trigger + render + **reason** | the full loop: agent views the flagged frames and rescues the run |

## The full Trigger-Render-Reason loop

`pipelines/lbm-cfd-agent.yaml` runs all three stages. Three shells — the writer
blocks in `Init_` until the SST reader connects (`RendezvousReaderCount=1`).

```bash
# --- shell 1: WRITER (needs iowarp + coeus build/bin, per Prerequisites) ---
jarvis ppl load yaml pipelines/lbm-cfd-agent.yaml
jarvis ppl kill && jarvis ppl run                  # waits for the reader

# --- shell 2: READER, once <script_location>/lbm_sst.bp.sst appears ---
#     ISOLATED adios2-only env (see Gotchas)
env -i HOME=$HOME bash -lc '
  source <spack>/share/spack/setup-env.sh; spack load adios2-coeus@vigil
  python3 <lbm-cfd>/consumer/lbm_sst_reader.py \
      --stream <script_location>/lbm_sst.bp --engine SST --max-steps 3 \
      --status-file /tmp/lbm_status.json --png-dir /tmp/lbm_pngs'

# --- shell 3: AGENT, once /tmp/lbm_status.json appears ---
#     ISOLATED system-python env; key from the environment, never on the CLI
env -i HOME=$HOME PATH=/usr/bin:/bin bash -c '
  export ANTHROPIC_API_KEY=sk-ant-...
  /usr/bin/python3 <lbm-cfd>/consumer/lbm_agent.py --model claude-haiku-4-5 \
      --status-file /tmp/lbm_status.json \
      --rescue-flag <script_location>/lbmcfd.bp.rescue \
      --stop-flag   <script_location>/lbmcfd.bp.stop'
```

`--max-steps` should equal `trigger_inspect_steps` (the gated stream ships
exactly that many). The reason step needs `agent_rescue: true`, and the flag
paths are `<script_location>/<out_file>.rescue|.stop` — note the base is the
**ADIOS output name** (`lbmcfd.bp`), so it is `lbmcfd.bp.rescue`, not
`lbmcfd.rescue`.

### Verified end-to-end (2026-07-16, Ares, single node)

```
engine  Trigger FIRED at step 2: variance(derive/VarVort) = 209.2 (threshold 1)
engine  SST flagged step 2/3/4 shipped        (window_left 3→2→1; nothing before the fire)
reader  recv #0 step=188 vort[±2.17e3] [GATED fired=1 trigger_stat=209.2  fire_step=2]
        recv #1 step=375 vort[±5.78e5] [GATED fired=0 trigger_stat=7.84e6 fire_step=2]
        recv #2 step=563 vort[±1.63e5] [GATED fired=0 trigger_stat=3.93e5 fire_step=2]
agent   "dense red/blue salt-and-pepper speckle at grid scale ... no coherent
         von Karman street"  ->  fire_rescue_simulation
sim     Agent RESCUE verdict at step 1688 -> revert to checkpoint 0,
        timesteps 6000→12000, speed 0.060
        final density 0.9466 / 1.0136 / 0.9952 at 12000/12000, 0 UNSTABLE after
```

## Gotchas

- **Both python consumers need isolated envs, for different reasons.** The
  reader needs adios2's python — don't load `iowarp@main` in its shell (it
  shadows numpy with a build for another python). The agent needs the *system*
  python (mcp + anthropic) — run it under `env -i`, because spack puts
  python3.12 packages on `PYTHONPATH` and python3.10 then imports the wrong
  `anyio`, which kills the MCP stdio transport.
- **`force_unstable` is the wrong case for a rescue** — it halts at step 400,
  leaving no room to recover. Use `force_unstable: false` with `steps: 6000`
  (u=0.12 diverges; one rescue doubling lands at 12000 / u=0.06 which is stable).
- Single-node run: the app's `mpirun` uses the **global** `server.list`; point it
  at the local node for a local run.
- `jarvis pkg configure force_unstable=false` is dropped (`false` == the menu
  default) — flip it via a dedicated YAML instead.
