# Incompact3d / Xcompact3d: Divergence & Numerical Instability Guide

A practical reference for identifying, reproducing, and fixing common sources of numerical blow-up in Xcompact3d simulations.

---

## 1. How the Code Detects Divergence

In `src/tools.f90`, the subroutine `test_speed_min_max` checks velocity extrema at runtime. When any velocity component exceeds a threshold (or becomes NaN), the code prints:

```
Velocity diverged! SIMULATION IS STOPPED!
```

and calls `MPI_ABORT`. The companion routines `compute_cfl` and `compute_cfldiff` report advective and diffusive CFL numbers but do **not** automatically adapt the time step - it must be set manually via the `dt` parameter in the `.i3d` file.

---

## 2. Common Instability Scenarios with Specific Configurations

### 2.1 Taylor-Green Vortex - Under-resolved DNS at High Re

**What it is:** The TGV is the primary benchmark case, transitioning from laminar to fully turbulent flow. The provided examples include `input_DNS_Re1600.i3d` (stable DNS) and `input_ILES_Re5000.i3d` (implicit LES).

**How to trigger divergence:**

Take the Re=1600 DNS input and increase the Reynolds number without adjusting the grid or using implicit LES dissipation:

```fortran
! In input.i3d - WILL DIVERGE
&BasicParam
  itype = 2            ! TGV case
  nx = 65
  ny = 65
  nz = 65
  dt = 0.001
  re = 5000.0          ! Increase from 1600 → under-resolved
  ilast = 100000
  istret = 0
  nclx1 = 1            ! Free-slip BCs (Neumann)
  nclxn = 1
  ncly1 = 1
  nclyn = 1
  nclz1 = 1
  nclzn = 1
/End

&NumOptions
  nu0nu = 4.0          ! Keep standard 6th-order (no extra dissipation)
  cnu = 0.44
/End
```

**Why it diverges:** At Re=5000 on a 65³ grid, the Kolmogorov scale is much smaller than the grid spacing. The 6th-order compact schemes have minimal numerical dissipation by design, so unresolved scales accumulate energy at the grid scale (aliasing), leading to blow-up around t ≈ 5–9 (near peak dissipation).

**Fix:** Either increase resolution to ~128³ or 257³ for DNS, or enable implicit numerical dissipation by adjusting `nu0nu` and `cnu` in `&NumOptions` (the ILES approach described by Dairay et al. 2017).

---

### 2.2 Excessive Time Step (CFL Violation)

**What it is:** Xcompact3d uses explicit time-stepping (Adams-Bashforth or Runge-Kutta). The time step `dt` must be set manually - there is no auto-adaptation.

**How to trigger divergence (any case):**

```fortran
! Take any working input, e.g., TGV Re=1600 on 129³
&BasicParam
  nx = 129
  ny = 129
  nz = 129
  dt = 0.01            ! 10× larger than typical stable value (~0.001)
  re = 1600.0
/End
```

**Why it diverges:** For the 6th-order compact schemes, the advective CFL condition is approximately CFL = u_max × dt / dx < ~0.7 (depending on scheme). With dx = π/128 ≈ 0.0245 and u_max ≈ 1.0, the maximum stable dt ≈ 0.017. But the actual stability limit is lower due to the RK3 temporal scheme and multi-dimensional coupling. In practice, dt ≈ 0.001–0.002 is typical for 129³ TGV.

**Diagnostic:** Before blow-up, the code reports CFL numbers via `compute_cfl`. Watch for CFL > 1.

**Fix:** The documentation states the optimal time step is found empirically. Start with a conservative dt and increase gradually.

---

### 2.3 Channel Flow with Incorrect Mesh Stretching

**What it is:** The periodic channel flow case (`Channel-Flow/`) uses mesh stretching in the wall-normal direction (y) controlled by the `beta` parameter and `istret`.

**How to trigger divergence:**

```fortran
&BasicParam
  itype = 3            ! Channel flow
  nx = 128
  ny = 129
  nz = 128
  dt = 0.001
  re = 180.0           ! Re_tau = 180
  istret = 1           ! Mesh stretching enabled
  beta = 0.1           ! Very aggressive stretching (too small)
  nclx1 = 0            ! Periodic in x
  nclxn = 0
  ncly1 = 2            ! No-slip at walls
  nclyn = 2
  nclz1 = 0            ! Periodic in z
  nclzn = 0
/End
```

**Why it diverges:** A very small `beta` produces extreme cell-size ratios between the wall region and channel center. The 6th-order compact schemes require moderate aspect ratios. With `beta = 0.1`, the near-wall cells are extremely thin while center cells are very coarse, and the CFL condition is violated locally near the walls (small dy → high CFL).

**Fix:** Use the provided Fortran utility `stretching_parameter_channel.f90` to determine an appropriate `beta`. For Re_tau=180 with ny=129, beta ≈ 0.259 (from the provided example `input_DNS_Re180_LR_explicittime.i3d`). As a rule of thumb, large positive beta → nearly uniform mesh; small positive beta → very stretched mesh.

---

### 2.4 Cylinder Flow with IBM - Spatial Resolution Mismatch

**What it is:** The Immersed Boundary Method case uses Lagrange interpolation or spline reconstruction to enforce no-slip at the solid/fluid interface. This can be sensitive to resolution and IBM parameter choices.

**How to trigger divergence:**

```fortran
&BasicParam
  itype = 5            ! Cylinder
  nx = 65              ! Very coarse
  ny = 65
  nz = 32
  dt = 0.005
  re = 1000.0          ! High Re for this coarse grid
/End

&IBMParam
  ivirt = 1            ! IBM enabled
  cex = 5.0
  cey = 6.0
/End
```

**Why it diverges:** The IBM introduces a forcing term in the N-S equations that is essentially a step function at the solid boundary. At high Re on coarse grids, the velocity gradients near the immersed surface become under-resolved. The combination of high-order schemes (which are susceptible to Gibbs-type oscillations near discontinuities) and the IBM forcing can generate spurious oscillations that grow unboundedly.

**Fix:** The provided example uses Re=300 with adequate resolution. For higher Re, increase resolution substantially and consider enabling implicit dissipation.

---

### 2.5 Single-Precision Compilation without DOUBLE_PREC

**What it is:** Xcompact3d can be compiled without the `-DDOUBLE_PREC` flag, using 32-bit floating point.

**How to trigger divergence:**

```bash
# Compile without double precision
make clean
make    # No -DDOUBLE_PREC flag
```

Then run any moderately challenging case (e.g., TGV Re=1600 on 129³ for long integration times).

**Why it diverges:** The Poisson solver operates in spectral space using FFTs. In single precision, round-off errors in the spectral Poisson solve accumulate over time, degrading the divergence-free condition. The modified wavenumber approach ensures machine-accuracy divergence-free solutions - but machine accuracy in single precision (~10⁻⁷) is much worse than double precision (~10⁻¹⁶). Over thousands of time steps, this can accumulate into significant mass conservation errors.

**Fix:** Always compile with `-DDOUBLE_PREC` for production simulations.

---

### 2.6 Gravity Current / Lock-Exchange with High Density Ratio

**What it is:** The lock-exchange case (itype=1) simulates gravity currents using the Boussinesq approximation or the Low-Mach Number (LMN) variable-density solver.

**How to trigger divergence:**

```fortran
&BasicParam
  itype = 1            ! Lock-exchange
  nx = 129
  ny = 65
  nz = 33
  dt = 0.001
  re = 5000.0          ! Reference example uses Re=2236
/End

&ScalarParam
  sc(1) = 1.0          ! Schmidt number
  ri(1) = 4.0          ! Large Richardson number (strong buoyancy)
/End
```

**Why it diverges:** High Richardson number combined with insufficient resolution creates strong density gradients (fronts) that the 6th-order scheme cannot resolve, leading to Gibbs-type oscillations in the scalar field that feed back into the momentum equations via the buoyancy term.

**Fix:** Reduce Re and Ri to values consistent with the resolution, or refine the mesh.

---

### 2.7 Inflow/Outflow Boundary Condition Issues

**What it is:** Non-periodic boundary conditions in the streamwise direction (e.g., for spatially evolving flows like boundary layers, TBL case).

**How to trigger divergence:**

```fortran
&BasicParam
  itype = 13           ! Turbulent Boundary Layer
  nclx1 = 2            ! Inflow
  nclxn = 2            ! Outflow (convective)
  inflow_noise = 0.5   ! 50% noise at inflow - far too high
/End
```

**Why it diverges:** Large inflow perturbation amplitudes (the docs recommend values between 0.0 and 0.1, i.e., 0–10% of reference velocity) introduce energy that the outflow convective boundary condition cannot evacuate fast enough. Reflections from the outflow boundary contaminate the domain and grow.

**Fix:** Use `inflow_noise` between 0.0 and 0.1. Ensure the domain is long enough for structures to develop and leave naturally.

---

### 2.8 Incompatible Grid Sizes with FFT Requirements

**What it is:** The spectral Poisson solver requires grid sizes that are products of small prime numbers (2, 3, 5). For non-periodic BCs, an extra mesh node is required (n+1).

**How to trigger failure:**

```fortran
&BasicParam
  nx = 100             ! Not a valid FFT size
  ny = 100
  nz = 100
  nclx1 = 0            ! Periodic
  nclxn = 0
/End
```

**Why it fails:** 100 = 2² × 5² is technically valid for FFTs, but the code may expect specific decompositions. For periodic BCs, n should be a product of powers of 2, 3, and 5. For non-periodic BCs, n-1 should satisfy this. Incompatible sizes can lead to incorrect Poisson solves, producing pressure fields that do not properly enforce incompressibility, leading to gradual or sudden divergence.

**Fix:** Use grid sizes like 32, 33, 48, 49, 64, 65, 96, 97, 128, 129, 192, 193, 256, 257, etc.

---

## 3. Systematic Diagnostic Checklist

When a simulation diverges, check these in order:

1. **CFL number**: Is the advective CFL (reported in output) staying below ~0.5–0.7? If not, reduce `dt`.

2. **Diffusive CFL**: For very fine grids, the diffusive stability limit dt < dx² / (2ν) per dimension can be more restrictive than the advective CFL.

3. **Grid size compatibility**: Ensure nx, ny, nz are compatible with the FFT requirements and boundary condition types.

4. **Resolution adequacy**: For DNS, estimate the Kolmogorov scale η = (ν³/ε)^(1/4). The grid spacing should satisfy dx ≈ η to 2η. For TGV at Re=1600, a 128³ grid is marginal; 256³ is well-resolved.

5. **Boundary conditions**: Verify the `nclX1/nclXn` settings are physically consistent (0=periodic, 1=free-slip, 2=Dirichlet/inflow-outflow).

6. **Stretching parameter**: For `istret > 0`, verify `beta` produces reasonable cell-size ratios using the stretching utility.

7. **Compilation flags**: Confirm `-DDOUBLE_PREC` is enabled.

8. **IBM parameters**: If using immersed boundaries, ensure sufficient resolution near the solid surface.

9. **Numerical dissipation**: For under-resolved simulations (LES), verify `nu0nu` and `cnu` are configured for implicit dissipation per Dairay et al. (2017).

---

## 4. Quick-Reference: Benchmark Cases and Their Stability Limits

| Case | itype | Provided Re | Typical Grid | Typical dt | Known Stability Concern |
|------|-------|-------------|-------------|------------|------------------------|
| Taylor-Green Vortex (DNS) | 2 | 1,600 | 128³–256³ | 0.001 | Under-resolution at higher Re without ILES |
| Taylor-Green Vortex (ILES) | 2 | 5,000 | 64³–128³ | 0.001 | Requires nu0nu/cnu tuning |
| Channel Flow (DNS) | 3 | 180 (Re_τ) | 128×129×128 | 0.001 | Stretching beta too small |
| Cylinder (DNS) | 5 | 300 | moderate | 0.005 | IBM + high Re |
| Lock-Exchange | 1 | 2,236 | 129×65×33 | 0.001 | High Ri buoyancy |
| TBL | 13 | low | elongated | 0.001 | Inflow noise, outflow reflections |
| Cavity | - | 14,084 | 2D adequate | small | High Re in confined geometry |
| 2D Hill | - | 1,000 | 3D adequate | small | Separation/reattachment |
| Wind Turbines | - | dimensional | large domain | small | Requires ILES approach |

---

## 5. References

- Laizet & Lamballais (2009). High-order compact schemes for incompressible flows. *J. Comp. Phys.*, 228(15), 5989–6015.
- Lamballais, Fortuné & Laizet (2011). Straightforward high-order numerical dissipation via the viscous term. *J. Comp. Phys.*, 230(9), 3270–3275.
- Dairay, Lamballais, Laizet & Vassilicos (2017). Numerical dissipation vs. subgrid-scale modelling for LES. *J. Comp. Phys.*, 337, 252–274.
- Bartholomew, Deskos, Frantz, Schuch, Lamballais & Laizet (2020). Xcompact3D: An open-source framework. *SoftwareX*, 12, 100550.
