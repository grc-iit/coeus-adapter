# Xcompact3d TGV Dissipation Trigger — Engine Integration

**Context:** Vigil `trigger-render-reason` pipeline — Xcompact3d "dark waste"
case (silent numerical dissipation on an under-resolved Taylor-Green Vortex).
**Engine side:** `src/hermes_engine.cc` (`TriggerType=dissipation`), branch `iowarp_2`.
**App side:** `Incompact3d/src/Case-TGV.f90` (`visu_tgv_init`) +
`2decomp-fft/src/io.f90` derived registrations.
**ADIOS2:** `~/ADIOS2` (github.com:hxu65/ADIOS2, branch `vigil`) — provides the
custom `gradient`, `mean`, `spectrum` derived operators (and `variance` used by
the gray-scott trigger). This is now the canonical clone; the previous copy
under `~/software/Incompact3d_2/Incompact3d/ADIOS2` has been removed.

---

## 1. Data flow

1. Xcompact3d writes `ux, uy, uz` (+ derived registrations) through the
   `solution-io` IO each output step. With `engine type="Plugin"` +
   `hermes_engine` in `adios2_config.xml`, the coeus engine intercepts the
   writes; field blobs land in CTE.
2. `HermesEngine::ComputeDerivedVariables()` (EndStep) executes every
   registered derived expression via `ApplyExpression` — including the op
   chains `mean(multiply(pow(magnitude(kx,ky,kz),2),0.5))` (`tke_mean`) and
   `mean(multiply(pow(magnitude(curl(ez,ey,ex)),2),0.5/h^2))` (`enst_mean`).
   Each rank's block reduces to a single value stored in CTE.
3. `EvaluateDissipationTrigger_()` pools the per-block means exactly
   (N_b-weighted, one 2-double Allreduce each), then forms the paper's
   metrics:

   ```
   eps_total = -(E_k(t) - E_k(t-Δt)) / Δt      Δt = TriggerOutputDt
   eps_phys  = 2 · nu · <enstrophy>            (exact for periodic flow)
   eps_frac  = (eps_total - eps_phys) / eps_total    # eps_num share
   nu_ratio  = eps_total / eps_phys                  # nu_eff / nu
   ```

4. **Yellow** (either `eps_frac ≥ TriggerYellowFraction` or
   `nu_ratio ≥ TriggerYellowNuRatio`): log-only escalation — a
   `trigger_yellow` event is appended to `TriggerLogFile` on the rising edge
   (hook for the asynchronous LLM text monitor).
5. **Red** (either red threshold): fires like the variance trigger — opens
   the SST inspect window (`TriggerInspectSteps` steps re-Put from CTE with
   the `vigil/trigger_*` scalars; unflagged steps ship nothing) and appends a
   `trigger_red` event. Fire-once by default; `TriggerRefire=true` re-arms.
6. Optional `TriggerMetricsLogFile`: rank 0 appends every step's
   `{step, ke, enstrophy, eps_total, eps_phys, eps_frac, nu_ratio}` — the
   data for the eval2-style timeline plot.

## 2. Engine parameters (ADIOS2 XML)

See `adios2-hermes-dissipation-trigger.xml` for a complete example.

| Parameter | Meaning | Default |
|---|---|---|
| `TriggerType` | `dissipation` selects this trigger (`variance` = gray-scott path) | `variance` |
| `TriggerKEVariable` | derived block-mean TKE (`tke_mean`) | required |
| `TriggerEnstrophyVariable` | derived block-mean enstrophy (`enst_mean`) | required |
| `TriggerNu` | kinematic viscosity (1/Re) | required |
| `TriggerOutputDt` | simulation time between output steps (`dt*ioutput`) | required |
| `TriggerYellowFraction` / `TriggerRedFraction` | `eps_frac` thresholds | 0.05 / 0.15 |
| `TriggerYellowNuRatio` / `TriggerRedNuRatio` | `nu_ratio` thresholds | 1.05 / 1.2 |
| `TriggerInspectSteps`, `TriggerRefire`, `TriggerLogFile`, `TriggerMetricsLogFile` | shared trigger knobs | 3 / false / trigger_log.jsonl / off |

## 3. Semantics & guards

- Metrics are undefined on the first evaluated step (no `dE_k/dt` yet) — no
  escalation is possible before the second output.
- If the TKE is not decaying (`eps_total ≤ 0`, early TGV phase), `eps_frac`
  and `nu_ratio` are forced to 0 — no spurious fires while energy is flat.
- `eps_frac` is clamped at 0 from below (over-resolved flow can give
  `eps_total < eps_phys` by the ~1% curl bias; that is not numerical
  dissipation).
- `vigil/trigger_stat` on shipped SST steps carries `eps_frac`.
- All per-run-unique actions (logs, scalar Puts) are guarded on the MPI rank,
  not the consensus rank (see VARIANCE_TRIGGER.md caveat).

## 4. Accuracy caveats (inherited from the derived ops)

- `curl`/`gradient` differentiate per writer block with clamped edges →
  block-seam bias under MPI decomposition; the documented ~1% enstrophy bias
  is from single-rank validation (`derived_comparison.md`). Trigger
  thresholds at 5%/15% have ample margin.
- The `0.5/h²` enstrophy scale assumes an **isotropic** grid.
- `ek_spectrum` is per-block: only a global spectrum with a single writer —
  do not trigger on it in decomposed runs.
- `skewness_dudx`/`flatness_dudx` are per-block ratio statistics and are not
  exactly poolable; treat them as diagnostics, not trigger inputs.

## 5. Status

- [x] Engine: `TriggerType=dissipation` implemented (`EvaluateDissipationTrigger_`,
      `ComputeGlobalBlockMean_`), sharing the gate/window/SST machinery with the
      variance trigger. Compiles on `iowarp_2`.
- [x] App: `Case-TGV.f90` + `2decomp-fft` registrations verified identical
      between `test/real_apps/Xcompact3d/` and the original working tree.
- [ ] Rebuild Xcompact3d/2decomp against `~/ADIOS2/install` and run a TGV case
      through the hermes engine end-to-end (fire expected once dissipation
      cascade onset crosses Yellow ~step 4200-equivalents per eval2).
- [ ] Bridge: ParaView pipeline for Q-criterion rendering of flagged steps.
