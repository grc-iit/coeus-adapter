# Vigil pipeline showcases

Jarvis-CD pipelines for the four applications wired for the Vigil
**trigger → render → reason** loop in the paper *"Trigger, Render, Reason:
Multimodal LLM Agents for Science-Aware Scientific Simulation Monitoring."*

Each pipeline runs the COEUS `hermes` engine over the IOWarp/clio-core runtime
(`clio_runtime` + `clio_cte`), computes an in-situ derived variable, and fires a
deterministic trigger that gates the SST render + LLM-agent stages. The
`clio_runtime`/`clio_cte` blocks and the per-node `CLIO_MEMFD_DIR` env are
identical across every file here — only the application package and its trigger
differ.

## Layout

```
vigil/
├── lbm/                              # LBM checkerboard instability (paper §5.6.1)
│   ├── lbm-cfd.yaml                  #   TRIGGER only — log-only fire (JSONL)
│   ├── lbm-cfd-sst.yaml              #   TRIGGER + RENDER — gated Catalyst/SST stream
│   └── lbm-cfd-agent.yaml            #   TRIGGER + RENDER + REASON — full agent rescue loop
├── gray-scott/                       # Gray-Scott regime identification (paper §5.6.2)
│   ├── gray-scott-warn-collapse.yaml #   single-node collapse-warning trigger → agent → early stop
│   └── delta/                        #   NCSA Delta multi-node variants (srun/PMIx, no inter-node ssh)
│       ├── gray-scott-warn.yaml      #     single-node Delta
│       ├── gray-scott-warn-mn.yaml   #     multi-node scale run (256 ranks; drives the paper's scale result)
│       └── gray-scott-warn-mn-dev.yaml #   iowarp@dev variant of the above
├── xcompact3d/                       # Xcompact3d numerical dissipation / "dark waste" (paper §5.6.3)
│   └── xcompact3d-dissipation.yaml   #   two-stage Yellow/Red dissipation trigger (log-only; SST opt-in)
└── lammps/                           # LAMMPS integration failure (paper §5.6.4)
    └── lammps-explosion.yaml         #   temperature trigger on velocity-Verlet blow-up (drift/stable via dt)
```

## Applications and triggers

| Showcase | `pkg_type` | Application code | Derived var | Trigger |
|---|---|---|---|---|
| LBM | `jarvis_coeus.lbm_cfd` | `test/real_apps/ascent-trame/examples/lbm-cfd` | `derive/VarVort` = variance(vorticity) | `variance` fires on first upward crossing of `trigger_threshold` (stable ~1e-5, unstable ~1e8) |
| Gray-Scott | `jarvis_coeus.adios2_gray_scott` | spack `adios2-gray-scott` (configs mirror `test/real_apps/gray-scott`) | `derive/VarV` = variance(V) | `variance` in **warn-on-collapse** mode: arms at `trigger_baseline_ratio`×baseline, warns when it falls back to `trigger_collapse_baseline_ratio`×baseline |
| Xcompact3d | `jarvis_coeus.Incompact3d` | `test/real_apps/Xcompact3d` (Case-TGV.f90 registers `tke_mean`/`enst_mean`) | `tke_mean`, `enst_mean` (block means) | `dissipation`, two-stage: **Yellow** `eps_frac≥0.05` or `nu_eff/nu≥1.05` (log); **Red** `≥0.15` / `≥1.20` (opens SST window). `trigger_nu`/`trigger_output_dt` auto-derive from `re`/`dt`/`io_frequency` |
| LAMMPS | `jarvis_coeus.lammps` | `test/real_apps/lammps` (+adios `dump custom/adios` build) | `vx` (raw field); `variance(vx)` ≈ `<vx²>` = T* | `variance` fires when `variance(vx) > trigger_baseline_ratio`×first-step baseline (temperature run-away) |

## Running

Pipelines load by path; the package resolves against the Jarvis package path, so
the new location does not change behavior:

```bash
# LBM — full trigger-render-reason loop
jarvis ppl load yaml test/jarvis/jarvis_coeus/pipelines/vigil/lbm/lbm-cfd-agent.yaml
jarvis ppl kill && jarvis ppl run            # writer blocks until the SST reader connects

# Gray-Scott — collapse-warning trigger → agent → early stop
jarvis ppl load yaml test/jarvis/jarvis_coeus/pipelines/vigil/gray-scott/gray-scott-warn-collapse.yaml
jarvis ppl kill && jarvis ppl run

# Xcompact3d — two-stage Yellow/Red dissipation trigger (log-only)
jarvis ppl load yaml test/jarvis/jarvis_coeus/pipelines/vigil/xcompact3d/xcompact3d-dissipation.yaml
jarvis ppl kill && jarvis ppl run

# LAMMPS — temperature trigger on velocity-Verlet blow-up
jarvis ppl load yaml test/jarvis/jarvis_coeus/pipelines/vigil/lammps/lammps-explosion.yaml
jarvis ppl kill && jarvis ppl run
```

Each YAML's header comment documents the reader/agent commands and the expected
fire step. See also:
- `docs/TRIGGER_RENDER_REASON_PIPELINE.md` — pipeline overview
- `docs/BUILD_AND_RUN_GRAY_SCOTT.md` — Gray-Scott build/run
- `docs/ARTIFACT_DESCRIPTION_DELTA.md` — Delta multi-node scale run
- `test/jarvis/jarvis_coeus/jarvis_coeus/lbm_cfd/README.md` — LBM three-tier pipelines
- `test/real_apps/Xcompact3d/README.md` — Xcompact3d dissipation trigger (build + run)
- `test/real_apps/lammps/RUNBOOK.md` — LAMMPS temperature trigger (build + run)

## Notes

- **Every YAML here declares `clio_runtime` + `clio_cte`** (identical blocks) so
  each pipeline stands alone; only the application package and its trigger differ.
- **Paths are host-specific** (`/mnt/common/hxu40/...`, `/dev/shm/clio_hxu40`,
  `lmp_bin`) — retarget `output_location`/`script_location`/`lmp_bin`/`CLIO_MEMFD_DIR`
  for your cluster.
- The parent `pipelines/incompact3d.yaml` and `pipelines/lammps.yaml` are the
  older **plain-run** versions (no trigger). The trigger-enabled Vigil versions
  now live here under `xcompact3d/` and `lammps/`; the plain ones can be removed
  once you've confirmed these.
