nical Review: Correctness and Standard-Practice Assessment of In-Situ Derived-Variable Triggers

## TL;DR
- Five of the seven table entries are essentially correct as intended (VarV, AddV, tke_mean, enst_mean, qcrit), but every one needs a wording fix because the "Physical meaning" column overstates precision — most notably calling `mean(0.5|u|^2)` "turbulent kinetic energy" (true only with zero mean flow), and labeling `variance(vorticity)` a stability detector without noting it is an ad hoc proxy reported in resolution-dependent lattice units, not a recognized LBM instability metric.
- The parallel/pooled-variance claim (AddV) is mathematically exact and is standard practice; the enstrophy–dissipation factor-of-2 bookkeeping (enst_mean) is internally consistent with the table's ½|curl u|² definition; the Q-criterion definition is textbook-correct.
- The two weakest entries are `variance(vorticity)` for LBM stability (defensible but non-standard) and `variance(vx)` as reduced temperature for LAMMPS (correct only for a monatomic, single-mass, zero-net-momentum system, and total-energy drift is the more standard integrator-stability check than a single velocity-component variance).

## Key Findings

The seven derived quantities are all cheap, single-scalar reductions computable in-situ with minimal communication — a legitimate design constraint that justifies "adequate" over "textbook-best" diagnostics. On a per-row basis:

1. **`derive/VarV = variance(V)` (Gray-Scott) — CORRECT WITH CAVEAT.** Spatial variance is a legitimate, if not the single dominant, order parameter for pattern sharpness/homogenization in reaction-diffusion systems. The critical caveat, correctly hinted but not stated, is that variance → 0 for *any* spatially uniform state — it cannot distinguish the trivial washout state (U=1, V=0) from other homogeneous states.

2. **`derive/AddV = add(V)` (pooling term) — CORRECT.** The law of total variance makes blockwise pooling exact; naive averaging of per-block variances is biased low because it drops the between-block term. This is standard in parallel/streaming variance algorithms and in-situ frameworks.

3. **`derive/VarVort = variance(vorticity)` (LBM stability) — IMPRECISE.** A reasonable ad hoc proxy (vorticity, being a velocity derivative, amplifies grid-scale/high-wavenumber content and so is sensitive to checkerboard modes), but not a recognized, named LBM stability diagnostic. The quoted O(10⁻⁵) magnitude is dimensional and resolution-dependent (lattice units), so it is not meaningful as an absolute number. Also, vorticity variance equals 2×enstrophy only if mean vorticity is zero.

4. **`tke_mean = mean(0.5|u|^2)` (Xcompact3d conservation) — CORRECT WITH CAVEAT.** ε_total = −dE_k/dt holds only with no forcing and no mean-shear production. The label "turbulent kinetic energy" is only correct in the zero-mean-flow case (HIT); in channel flow it is total, not turbulent, KE.

5. **`enst_mean = mean(0.5|curl u|^2)` (Xcompact3d conservation) — CORRECT.** With enstrophy defined as Ω = ⟨½|ω|²⟩, ε_phys = 2νΩ = ν⟨ω²⟩ is exactly right and the factor-of-2 is self-consistent. The residual ε_total − ε_phys as a measure of numerical dissipation is a legitimate, established technique. There is a genuine definitional ambiguity in the literature (some authors omit the ½), which should be flagged.

6. **`derive/VarVx = variance(vx)` (LAMMPS positivity/bound) — CORRECT WITH CAVEAT.** T* = ⟨v_x²⟩ in LJ reduced units holds only for a monatomic single-mass system at zero net momentum, and variance(vx) = ⟨v_x²⟩ only when ⟨v_x⟩ = 0. It is a valid but noisier single-component estimator. "Positivity/bound" is a questionable trigger category; total-energy drift is the more standard integrator-stability check.

7. **`derive/qcrit` = Q-criterion (render only) — CORRECT.** Q = ½(‖Ω‖² − ‖S‖²), the second invariant of ∇u, from Hunt, Wray & Moin (1988); the most widely used vortex-identification criterion, requiring an arbitrary threshold.

## Details

### 1. Variance of a reactant field (Gray-Scott)

The Gray-Scott model (Gray & Scott; popularized by Pearson 1993, "Complex Patterns in a Simple System," *Science*) is the canonical two-species reaction-diffusion system: ∂u/∂t = D_u∇²u − uv² + F(1−u); ∂v/∂t = D_v∇²v + uv² − (F+k)v. It exhibits spots, stripes, labyrinths, and "pattern death" (self-replication and collapse) as F, k vary.

Spatial variance (equivalently spatial standard deviation) of a concentration field is a mathematically sound measure of spatial heterogeneity: a sharp, spotted V-field has high variance; a homogenized field has low variance. It is used in practice as an order parameter, but it is not the single dominant choice in the pattern-formation literature. The more physically informative and more common characterizations are:
- **Structure factor / spatial power spectrum** S(k): identifies the dominant wavelength (the critical wavenumber from linear/Turing analysis), distinguishing spots from stripes and tracking coarsening.
- **Interface length / gradient norm** ⟨|∇V|²⟩: measures total interface, sensitive to pattern sharpness.
- **Shannon entropy** of the field histogram and **invariant descriptors** (e.g., resistance-distance histograms with Wasserstein kernels, used in Turing-pattern parameter inference).

The essential caveat — correctly implied by "collapsing toward zero as the field homogenizes" but not fully stated — is that variance is **blind to which homogeneous state** the system reaches. Var(V) → 0 both for the trivial washout state (U→1, V→0) and for any other spatially uniform state. Thus variance detects *homogenization* (loss of pattern) but not *which* attractor was reached; a companion mean (which AddV supplies) is required to disambiguate. As a cheap trigger to flag "pattern has died / field has homogenized," variance is adequate and defensible; as a *characterization* of the pattern, a structure factor is the domain-expert choice.

**Suggested wording:** "Spatial variance of the reactant field, a measure of pattern heterogeneity: large while reactive spots are sharp, collapsing toward zero as the field homogenizes. Note: variance vanishes for *any* uniform state, so it detects loss of pattern but not which homogeneous state (trivial U=1,V=0 or otherwise) is reached; pair with the mean to disambiguate."

### 2. Parallel/blockwise pooled variance (AddV)

The statistical claim is exactly correct and is a direct consequence of the **law of total variance** (variance decomposition / "Eve's law"): total variance = mean of within-group variances + variance of the group means. In the form the table uses,

Var_global = (1/N) Σ_b n_b [ Var_b + (mean_b − mean_global)² ],

the second term is the **between-block** contribution. Naively averaging per-block variances omits this term and is therefore **biased low** — it is only correct when all block means are equal. This is the same "parallel-axis theorem" identity noted in the exact-pooled-variance literature.

This is bedrock standard practice in numerically stable, one-pass/parallel variance computation:
- **Chan, Golub & LeVeque (1982, COMPSTAT; 1983, *The American Statistician* 37:242–247), "Algorithms for Computing the Sample Variance: Analysis and Recommendations"** — the pairwise/parallel combining formula S_{1,m+n} = S_{1,m} + S_{m+1,m+n} + [m/(n(m+n))](n/m·T_{1,m} − T_{m+1,m+n})², combining two subsamples of arbitrary size using their sums and sums-of-squares. This is the reference for the parallel update and its rounding-error analysis (O(log N) parallel steps via pairwise summation).
- **Welford (1962)** and **West (1979)** / **Hanson (1975)** for the equivalent single-pass online update; **Youngs & Cramer (1971)** for an early updating formula; **Pébay (2008)** for the extension to arbitrary-order moments and covariances.

Requiring per-block sum (→ mean) and count in addition to per-block variance is exactly what these combining formulas need. The mapping of AddV as a "pooling term" that supplies the per-block means N_b is therefore correct and well-motivated. This pattern is precisely what distributed in-situ analysis frameworks implement for reductions; ADIOS2 provides "derived variables" (with operations including add, magnitude, curl, and, in recent releases, trigonometric functions, multiply, divide, and power) evaluated in-situ, and the broader in-situ ecosystem (Ascent, ParaView Catalyst, VTK-m) performs distributed statistical reductions of exactly this kind.

**Verdict: CORRECT.** No wording fix required, though "supplies per-block means N_b" could be tightened to "supplies per-block sums, which with block counts yield the between-block term of the law of total variance."

### 3. Variance of vorticity as an LBM stability detector

This is the entry that most needs qualification. The literature on LBM numerical instability is well developed, and the **recognized, standard indicators are not vorticity variance**:
- **Negative / non-finite distribution functions.** The practical operational detector: BGK-LBGK instabilities are frequently preceded by populations going negative; positivity of the equilibrium distribution is tied to stability.
- **von Neumann / linear stability analysis of high-wavenumber modes.** Sterling & Chen (1996, "Stability Analysis of Lattice Boltzmann Methods") established that τ > ½ is required and that a maximum stable mean velocity exists; instabilities occur in the high-wavenumber range (Ricot et al. 2009, *JCP*), where acoustic and spurious modes interact.
- **Non-equilibrium (f − f^eq) / non-hydrodynamic "ghost" mode norms.** Latt & Chopard (2006) regularization and multiple-relaxation-time (MRT) / recursive-regularized approaches stabilize precisely by filtering the non-equilibrium part; monitoring non-equilibrium moment norms is the principled mesoscopic detector.
- **Total kinetic-energy / local computation blow-up.**

Where vorticity enters, it is normally a **physics/validation** quantity (dissipation, enstrophy spectra, benchmark comparison), not a stability monitor. However, the physical premise behind the table's choice is sound and is explicitly documented: grid-scale checkerboard / odd-even modes do manifest in the vorticity field. Astoul, Wissocq, Boussuge, Sengissen & Sagaut (2020, *J. Comput. Phys.* 420:109645) show these modes "have a real pulsation ω_r very close to ω_r = π so that their amplitude is inverted at each iteration. Here, it is possible to build a sensor based on the vorticity product between two iterations, which has to be negative." Because vorticity is a first derivative of velocity, it weights high-wavenumber content by ~k and is therefore genuinely sensitive to grid-scale oscillations — the reasoning is valid even though the *recognized* construction is a sign-based time-product (plus an off-equilibrium moment decomposition for modes invisible in macroscopic fields), not a variance/RMS threshold.

Two further precision issues:
- **Vorticity variance ≠ enstrophy in general.** Var(ω) = ⟨ω²⟩ − ⟨ω⟩², whereas enstrophy = ½⟨ω²⟩ (or ⟨ω²⟩ depending on convention). Var(ω) = 2×enstrophy only when the mean vorticity ⟨ω⟩ = 0. The table should not conflate the two.
- **The O(10⁻⁵) magnitude is not meaningful as stated.** Vorticity in LBM is dimensional and expressed in lattice units (∝ 1/timestep); enstrophy ∝ 1/time². Its numerical magnitude depends entirely on grid resolution and the lattice→physical unit conversion (Latt 2008 conversion framework). A bare "O(10⁻⁵) for stable channel flow" is not comparable across resolutions or setups unless non-dimensionalized (e.g. normalized by δt or a reference enstrophy). What *is* meaningful and defensible is the **relative** statement — a rise "by orders of magnitude" when checkerboard modes appear.

**Suggested wording:** "Spatial variance of the vorticity field, used as an inexpensive proxy for grid-scale velocity oscillation. Because vorticity amplifies high-wavenumber content, it is sensitive to checkerboard / odd-even modes; a rise of several orders of magnitude flags incipient instability. Note this is an ad hoc proxy, not a standard LBM stability metric (canonical detectors are negative populations, non-equilibrium moment norms, and von Neumann analysis of high-k modes); the absolute value is in lattice units and resolution-dependent, so only relative change is meaningful. Equal to 2×enstrophy only if mean vorticity is zero." **Verdict: IMPRECISE.**

### 4. TKE and the dissipation balance (Xcompact3d)

The physics relations are standard (Pope, *Turbulent Flows*; Frisch, *Turbulence*):
- **E_k = ⟨½|u|²⟩** is the volume-averaged kinetic energy. The label "turbulent kinetic energy" is only correct when there is no mean flow — i.e. in homogeneous isotropic turbulence (HIT) with zero mean velocity, where total and turbulent KE coincide. In channel flow or any flow with a mean, TKE requires subtracting the mean field, k = ½⟨u′·u′⟩ ≠ ½⟨|u|²⟩. **This is the most important label error in the table.**
- **dE_k/dt = −ε** holds only with no forcing and no mean-shear production (again, decaying HIT or the Taylor-Green vortex). So ε_total = −dE_k/dt is correct in that setting; in forced or sheared flows there are production/input terms.
- **ε = 2ν⟨S_ij S_ij⟩** exactly (S = strain-rate tensor), and under homogeneity/periodicity ⟨ω_iω_i⟩ = 2⟨S_ijS_ij⟩, so ε = ν⟨ω²⟩ = ν⟨ω_iω_i⟩. The equality of strain- and enstrophy-based dissipation holds only up to boundary/divergence terms that vanish under homogeneity — the topologies of the strain and enstrophy fields differ even though their means match.
- With enstrophy Ω = ⟨½|ω|²⟩, **ε_phys = 2νΩ = ν⟨ω²⟩** — the factor-of-2 in the table is internally consistent with the ½|curl u|² definition. **Definitional ambiguity flag:** many authors define enstrophy without the ½ (as ⟨|ω|²⟩ or ⟨ω_iω_i⟩), in which case ε = ν⟨enstrophy⟩. The table's convention is self-consistent but readers must not mix conventions.

The **residual ε_total − ε_phys as numerical dissipation** is a legitimate, established measure. The canonical validation is the Taylor-Green vortex (Brachet et al. 1983 reference solution; TGV at Re = 1600 is the standard benchmark), where −dE_k/dt computed directly is plotted against 2ν×enstrophy; the gap at coarse resolution is the numerical-dissipation signature. In DeBonis's high-resolution finite-difference TGV study (AIAA 2013-0382), the directly computed peak kinetic-energy-dissipation rate at t*≈9 on a 64³ grid has an error of 9.8% versus the spectral reference, while the 128³, 256³, and 512³ grids are all within two percent; the enstrophy-based dissipation rate is significantly lower than the directly computed rate at low resolution and converges to it as the grid is refined. The physical-space quantification technique is due to **Domaradzki et al.** and **Schranner et al. (2015, *Computers & Fluids* 114:84–97)**, which treat the solver as a black box and extract the numerical dissipation rate as the residual of the discretized kinetic-energy equation; this has been applied specifically to TGV implicit LES.

This is directly relevant to Xcompact3d/Incompact3d specifically. Xcompact3d (Bartholomew, Deskos, Frantz, Schuch, Lamballais & Laizet 2020, *SoftwareX* 12:100550; based on Laizet & Lamballais 2009, *JCP* 228:5989–6015; Lele 1992 sixth-order compact schemes; 2D decomposition Laizet & Li 2011) implements an implicit LES / "under-resolved DNS" strategy in which **artificial dissipation is introduced through the discretization of the viscous term** — Lamballais, Fortuné & Laizet (2011, *JCP* 230:3270–3275) and Dairay, Lamballais, Laizet & Vassilicos (2017, *JCP* 337:252–274) show this targeted numerical dissipation is equivalent to a spectral vanishing viscosity. Because the scheme's numerical dissipation is deliberate and spectrally targeted, the ε_total vs 2νΩ residual is exactly the right in-situ diagnostic for monitoring how much dissipation is numerical vs physical. The canonical Xcompact3d TGV case (input_ILES_Re5000) is precisely this balance.

**Suggested wording (tke_mean):** "Volume-averaged kinetic energy E_k = ⟨½|u|²⟩. In flows with zero mean velocity (HIT, Taylor-Green) this equals the turbulent kinetic energy and, absent forcing/mean-shear production, its time derivative gives the total dissipation ε_total = −dE_k/dt. With a mean flow (e.g. channel), it is total, not turbulent, KE and the balance acquires production/input terms." **Verdict: CORRECT WITH CAVEAT.**

**Suggested wording (enst_mean):** "Volume-averaged enstrophy Ω = ⟨½|ω|²⟩; under homogeneity/periodicity the physical dissipation is ε_phys = 2νΩ = ν⟨ω²⟩, so the residual against ε_total measures numerical (scheme/filter) dissipation. Convention note: enstrophy is here defined with the ½; authors omitting it write ε = ν⟨enstrophy⟩." **Verdict: CORRECT.**

### 5. variance(vx) as reduced temperature (LAMMPS)

The equipartition relation is correct: at thermal equilibrium, ⟨½m v_x²⟩ = ½k_B T per translational degree of freedom, so T = m⟨v_x²⟩/k_B, and in LJ reduced units (m=1, k_B=1) T* = ⟨v_x²⟩. Several qualifications apply, all of which the table glosses:
- **variance(vx) = ⟨v_x²⟩ only when ⟨v_x⟩ = 0**, i.e. zero net momentum / center-of-mass motion removed (LAMMPS `fix momentum` or `velocity ... zero linear`). Otherwise variance = ⟨v_x²⟩ − ⟨v_x⟩² undercounts.
- **Single-component estimator is valid but noisier.** LAMMPS `compute temp` uses the full 3N (or 3N−3 after COM removal) degrees of freedom: T = 2·KE/((3N − N_fixDOF)k_B). Using one velocity component uses ~N/3 of the information, so relative statistical fluctuations are larger by √3.
- **Mass weighting.** ⟨v_x²⟩ ∝ T only for a single-mass (monatomic) system; for multi-species systems temperature is Σ½m_i v_{i}² summed over the mixture, so an unweighted variance(vx) is not proportional to T.
- **Degrees-of-freedom correction.** LAMMPS applies the 3N−3 (vs 3N) correction via `extra_dof`/`compute_modify`, and subtracts DOF for constraints (fix shake, fix rigid). A raw component variance does none of this.
- **"Runs away when the integrator diverges" is a sound *qualitative* instability signal** — a too-large timestep causes atoms to interpenetrate steeply repulsive walls, velocities blow up (the classic "lost atoms" / "bond atoms missing" failure), and any velocity-based quantity diverges. But the **more standard and more sensitive integrator-stability check is total-energy drift** in the NVE ensemble. Symplectic Verlet integrators conserve a shadow Hamiltonian, and community guidance is to keep drift below ~10 meV/atom/ps (≈1 meV/atom/ps being good); temperature runaway is a *lagging, coarse* indicator by comparison.
- **Trigger category.** Labeling this "Positivity/bound" is awkward. Variance(vx) is intrinsically non-negative, so it is not a positivity check on a physical field; and T has no hard upper bound in NVE. It is better described as a **stability/conservation** monitor (energy/temperature runaway) than a positivity/bound trigger.

**Suggested wording:** "Velocity-component variance ⟨v_x²⟩ (valid only at zero net momentum, ⟨v_x⟩=0). For a monatomic single-mass system it is proportional to temperature (T* = ⟨v_x²⟩ in LJ units), a valid but ~√3 noisier estimator than the full-DOF compute temp; diverges when the integrator becomes unstable (too-large timestep → 'lost atoms'). Consider total-energy drift as the primary integrator-stability check." **Verdict: CORRECT WITH CAVEAT** (and reclassify the trigger as stability, not positivity/bound).

### 6. Q-criterion

The definition is textbook-correct. Q = ½(‖Ω‖² − ‖S‖²), where Ω and S are the antisymmetric (rotation-rate) and symmetric (strain-rate) parts of the velocity-gradient tensor ∇u and ‖·‖ is the Frobenius norm; Q is the **second invariant** of ∇u (for incompressible flow, where the first invariant P = tr(∇u) = 0). Q > 0 marks regions where rotation dominates strain, i.e. vortex cores. Origin: **Hunt, Wray & Moin (1988), "Eddies, streams, and convergence zones in turbulent flows," CTR Report CTR-S88** (with the auxiliary condition of a local pressure minimum, usually omitted in practice).

It is among the **most widespread and well-known** vortex-identification methods — as Kovács & Balla note (*Int. J. Heat Fluid Flow*), "probably one of the most widespread, and well-known vortex identification methods is the Q-criterion, which is Galilean invariant, and it is based on the velocity-gradient tensor." It should be compared to the alternatives: the **Δ-criterion** (Chong, Perry & Cantwell 1990, complex eigenvalues of ∇u), **λ₂** (Jeong & Hussain 1995, second eigenvalue of S² + Ω²), the **swirling strength λ_ci** (Zhou et al. 1999), and the newer **Liutex/Rortex** family (Liu et al. 2018), which was introduced specifically to remove the shear contamination and threshold-sensitivity of Q/λ₂. Chakraborty, Balachandar & Adrian (2005) showed these criteria give approximately equivalent structures for turbulent flows. The well-known limitation — correctly implying "render only" is appropriate — is that Q **requires an arbitrary threshold** (Q > some fraction of Q_max, or normalized by mean strain); the iso-surface shape depends on that subjective choice, which is why it is suited to visualization/rendering rather than to a quantitative trigger.

**Verdict: CORRECT.** Wording is fine; optionally add "(requires an arbitrary iso-surface threshold, hence render-only rather than trigger)."

### Trigger-category mapping (V&V perspective)

From a verification-and-validation standpoint (Oberkampf & Roy 2010, *Verification and Validation in Scientific Computing*; Roache), the categories are mostly sensible:
- **Statistical (VarV)** — appropriate: a distributional summary of a field.
- **Stability (VarVort)** — appropriate *intent*, but the chosen quantity is a proxy (see §3).
- **Conservation (tke_mean, enst_mean)** — appropriate and well-grounded: the energy–enstrophy balance is a conservation/consistency check, and the numerical-dissipation residual is a solution-verification quantity in the Oberkampf–Roy sense.
- **Positivity/bound (VarVx)** — mislabeled; this is really a stability/conservation (energy-runaway) check (see §5).
- **Render only (qcrit)** — appropriate.

These monitors align with the in-situ / computational-steering philosophy: cheap scalar reductions that let an automated agent detect when a run has left its validated regime (homogenization, checkerboard onset, non-conservation, integrator blow-up) without halting the simulation. This is exactly the kind of runtime credibility monitoring V&V practice recommends.

### More standard/robust alternatives (domain-expert choices)

Given the single-scalar, minimal-communication constraint, the table's choices are reasonable, but a domain expert would consider:
- **Gray-Scott:** structure factor (dominant wavenumber) or total interface length / ⟨|∇V|²⟩ to characterize the pattern, plus the mean to disambiguate the homogeneous state.
- **LBM:** the norm of the non-equilibrium distribution ‖f − f^eq‖ or a non-hydrodynamic-moment sensor; fraction of negative populations; both are the recognized stability signals.
- **MD:** total-energy drift in NVE (or the thermostat's conserved quantity in NVT) rather than a single velocity-component variance.
- **DNS:** the dissipation-based resolution criterion k_max·η (η = Kolmogorov scale). In DNS aimed at maximizing Reynolds number, typical values are 1 ≤ k_max η ≤ 2 (Gotoh & Yeung 2013), with k_max η ≈ 1.5 typical for adequately-resolved isotropic-turbulence DNS — a direct in-situ resolution-adequacy check to complement the energy–enstrophy residual.

All of these are still cheap scalar reductions, so several could be adopted without violating the in-situ design constraint.

## Recommendations

1. **Fix the labels now (zero cost, high value).** Apply the suggested wording fixes above. The two mandatory ones: (a) relabel `mean(0.5|u|^2)` as "volume-averaged kinetic energy (= TKE only with zero mean flow)"; (b) downgrade `variance(vorticity)` from "amplitude of grid-scale oscillation" to "ad hoc proxy for grid-scale oscillation, in resolution-dependent lattice units."

2. **Reclassify VarVx's trigger** from "Positivity/bound" to "Stability/Conservation," and — if the code already tracks it — prefer or add **total-energy drift** as the primary MD integrator-stability trigger (threshold guidance: <~10 meV/atom/ps, ideally ~1 meV/atom/ps). Threshold to change the recommendation: if energy drift is unavailable in-situ but temperature is, keep variance(vx) but document that it is a lagging indicator.

3. **Non-dimensionalize the LBM indicator.** Report VarVort normalized (by δt², by a reference enstrophy, or in wall units) and trigger on **relative** change (e.g. ≥ 2 orders of magnitude rise over a moving baseline) rather than an absolute O(10⁻⁵) threshold. Benchmark that would change the design: if a von Neumann analysis or non-equilibrium-moment norm is cheaply available, prefer it as the primary stability trigger and keep VarVort as a corroborating render/diagnostic.

4. **State the Gray-Scott ambiguity explicitly** and pair VarV with the block mean (already available via AddV) so the trigger can distinguish washout (V→0) from a non-trivial uniform state.

5. **For the DNS conservation trigger, add k_max·η** as a companion resolution-adequacy scalar; if k_max η drops below ~1.5 the ε_total−ε_phys residual should be interpreted as under-resolution, not merely scheme dissipation.

6. **Keep the enstrophy convention explicit** (½ included) wherever ε_phys = 2νΩ is used, to prevent factor-of-2 errors when comparing to sources that omit the ½.

## Summary Table of Verdicts

| Derived variable | Expression | Verdict | Principal fix / caveat |
|---|---|---|---|
| `derive/VarV` | variance(V) | **CORRECT WITH CAVEAT** | Variance vanishes for *any* uniform state; detects homogenization, not which state. Structure factor is the richer alternative. |
| `derive/AddV` | add(V) | **CORRECT** | Exact via law of total variance (Chan–Golub–LeVeque). Naive mean-of-variances is biased low. |
| `derive/VarVort` | variance(vorticity) | **IMPRECISE** | Ad hoc proxy, not a standard LBM stability metric; O(10⁻⁵) is lattice-unit/resolution-dependent — use relative change only; = 2×enstrophy only if ⟨ω⟩=0. |
| `tke_mean` | mean(0.5·\|u\|²) | **CORRECT WITH CAVEAT** | "Turbulent" KE only with zero mean flow (HIT/TGV); ε_total=−dE_k/dt only without forcing/mean shear. |
| `enst_mean` | mean(0.5·\|curl u\|²) | **CORRECT** | ε_phys=2νΩ=ν⟨ω²⟩ self-consistent with the ½ convention; flag literature's ½-omission ambiguity; residual = numerical dissipation (Schranner/Domaradzki; Xcompact3d ILES via viscous-term dissipation). |
| `derive/VarVx` | variance(vx) | **CORRECT WITH CAVEAT** | T*=⟨v_x²⟩ only for monatomic single-mass, ⟨v_x⟩=0; ~√3 noisier than compute temp; reclassify trigger as stability; prefer energy drift. |
| `derive/qcrit` | Q-criterion | **CORRECT** | Q=½(‖Ω‖²−‖S‖²), 2nd invariant of ∇u (Hunt, Wray & Moin 1988); needs arbitrary threshold → render-only is right. |

## Caveats

- The finding that "variance of vorticity" is **not** a named LBM stability metric rests on the absence of such usage across the surveyed stability literature plus indexed textbook contents (Krüger et al. 2017, *The Lattice Boltzmann Method: Principles and Practice*); it is a firm but not absolutely provable negative.
- The O(10⁻⁵) magnitude could not be independently reproduced; it is flagged as non-meaningful in absolute terms because it is dimensional and resolution-dependent, but its *relative* use as a change-detector is sound.
- Several relations (dE_k/dt = −ε; ε = ν⟨ω²⟩) are exact only under specific idealizations (no forcing/mean shear; homogeneity/periodicity). The table's conservation triggers are valid for the HIT/TGV-type setups Xcompact3d is validated on but would need extra terms in wall-bounded or forced runs.
- Assessment of software specifics (exact ADIOS2 derived-variable semantics for `variance`/`add`, and how block reductions are combined in this particular pipeline) is based on ADIOS2 documentation and general in-situ practice; the exact combining implementation in the user's pipeline was not inspected and should be confirmed to ensure the between-block term is actually included (the whole point of AddV).
