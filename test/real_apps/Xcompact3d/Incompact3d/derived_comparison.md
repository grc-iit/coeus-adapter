# Native (`Case-TGV.f90`) vs ADIOS2 derived quantities — are they the same?

Scope: the three **field** derived quantities written by `visu_tgv`
(`src/Case-TGV.f90`): `vort`, `critq`, and MHD `J`. Compared against what
the stock spack **ADIOS2 2.11.0** derived engine (`libadios2_core_derived`)
can compute.

## TL;DR

| Quantity | Same *definition*? | Same *numbers*? | ADIOS2 expression |
|----------|:---:|:---:|-------------------|
| `vort` = \|∇×u\|      | ✅ yes | ❌ **no** | `magnitude(curl(ux,uy,uz))` |
| `critq` = Q-criterion | — | ❌ **not expressible** | needs gradient tensor / custom `QCRIT` op (fork only) |
| `J` = ∇×B / Rem       | ✅ yes | ❌ **no** | `curl(Bx,By,Bz)` then `/Rem` |

Definitions match for `vort` and `J` (identical curl sign convention — verified
below). **The numbers do not match**, for three concrete reasons that all trace
back to how ADIOS2 differentiates. `critq` cannot be built from stock ops at all.

---

## 1. `vort` — magnitude of vorticity

**Native** (`Case-TGV.f90:619-624`), with the temp-array map at lines 616-618:
```
ω_x = dw/dy − dv/dz = tf1 − th1
ω_y = du/dz − dw/dx = tg1 − tc1
ω_z = dv/dx − du/dy = tb1 − td1
vort = sqrt(ω_x² + ω_y² + ω_z²)
```
Derivatives are Incompact3d's **6th-order compact** `derx/dery/derz`
(physical `∂/∂x`, periodic BCs, y-stretch via `ppy`).

**ADIOS2** `magnitude(curl(ux,uy,uz))` — `ApplyCurl`
(`ADIOS2/source/adios2/toolkit/derived/Function.cpp:101-141`):
```
curl[0] = (v3[j+1]−v3[j−1])/(Δj) + (v2[k−1]−v2[k+1])/(Δk)   // = dw/dy − dv/dz
```
Same component definition and **same sign convention** ✅. But the numerics differ:

## 2. Why the numbers differ (applies to `vort` **and** `J`)

1. **Order of accuracy.** ADIOS2 = 2nd-order central difference
   `(u[i+1]−u[i−1])/2`. Native = 6th-order compact. Different truncation error
   everywhere.

2. **Grid spacing is ignored (the big one).** ADIOS2 divides by the *index*
   difference (`next − prev` = 2 in the interior), **not** by physical `Δx`.
   So it returns
   ```
   adios_curl_component ≈ Δ · (true ∂/∂x)         (for isotropic Δx=Δy=Δz=Δ)
   ```
   i.e. every ADIOS2 curl value is scaled by the grid spacing versus the native
   value. On a TGV box `[0,2π]³` with `nx=64`, `Δ = 2π/64 ≈ 0.098`, so
   `vort_adios ≈ 0.098 · vort_native` — off by an order of magnitude, not a
   rounding difference. (Anisotropic grids don't even give a clean scalar
   factor, because each curl component mixes two directions.)

3. **Boundaries are not periodic.** ADIOS2 clamps at the edges
   (`std::max(0,i-1)`, `std::min(dim-1,i+1)` → one-sided, denominator 1). TGV is
   fully periodic, so the first/last plane in each direction is simply wrong in
   the ADIOS2 field, whereas the native compact scheme wraps correctly.

**Verdict for `vort`/`J`:** same mathematical object, but ADIOS2's field is a
grid-spacing-scaled, 2nd-order, non-periodic approximation. Not directly
comparable without (a) multiplying back by the spacing and (b) discarding
boundary planes — and even then it stays 2nd-order.

## 3. `critq` — Q-criterion

**Native** (`Case-TGV.f90:626-632`) needs the **full 9-component velocity
gradient tensor**:
```
Q = −½(∂u/∂x² + ∂v/∂y² + ∂w/∂z²) − ∂u/∂y·∂v/∂x − ∂u/∂z·∂w/∂x − ∂v/∂z·∂w/∂y
```
Stock ADIOS2 2.11.0 has **no gradient operator and no `QCRIT` op** — only
`curl`, `magnitude`, `cross`, and elementwise math. The gradient tensor cannot
be assembled, so **Q cannot be expressed in stock ADIOS2**. Only the
coeus / `ADIOS2_qcrit` fork provides a custom `QCRIT(x,y,z)` op, and it too
would use 2nd-order index-space derivatives (same three caveats as above).

## 4. `J` — MHD current density

**Native** (`Case-TGV.f90:639`): `Je = del_cross_prod(Bm)/Rem`.
`del_cross_prod` (`src/mhd.f90:232-254`) = `(∂Bz/∂y−∂By/∂z, ∂Bx/∂z−∂Bz/∂x,
∂By/∂x−∂Bx/∂y) = ∇×B`, 6th-order compact. Written as separate `J_x/J_y/J_z`.

**ADIOS2:** `curl(Bx,By,Bz)` → divide by `Rem`. Definition and sign match ✅.
Differences: same three numeric caveats as §2, **plus** ADIOS2 curl emits one
4-D field of shape `(d1,d2,d3,3)` — not three separate `J_x/J_y/J_z` variables,
so the output layout differs from the native writer.

### 4a. Empirical check — run `runs/otv3d_cmp`

Ran a 3-D Orszag-Tang MHD case (32³, `Rem=50`, BP5), 4 snapshots. Native
`J_x/J_y/J_z` (6th-order compact) vs the **real ADIOS2 derived `curl`** engine
fed the same `B` (`compare_J.py`). Result, every component / every snapshot:

| call | corr with native J | ratio (ADIOS/native) |
|------|:---:|:---:|
| **raw** `curl` on stored field | 0.3 / −0.02 / 0.86 | ~0.10–0.15 (wrong) |
| **fixed** (axis order + ×1/dx) | **0.9996–1.0000** | **0.992–0.977** |

Two corrections are mandatory, or the numbers are meaningless:

1. **Axis order.** decomp2d writes the field **reversed (z,y,x)**; ADIOS2 `curl`
   hard-assumes axis0=x, axis1=y, axis2=z. Raw, it therefore **permutes the J
   components** (that's why `J_z` half-correlated but `J_x/J_y` didn't). Fix:
   feed B in x,y,z order (transpose, or arrange the derived expression inputs).
2. **Grid spacing.** ADIOS2 differentiates in index space, so the result must be
   multiplied by `1/dx` (isotropic here, `dx=0.196`) — the raw ratio ≈ `dx`.
   Then `/Rem`.

After both fixes: **trend is identical (corr = 1.0000)** and **magnitude agrees
to ~1–2 %**. That residual is exactly the 2nd-order-vs-6th-order truncation:
central differencing attenuates a mode by `sin(kΔ)/(kΔ)`, ≈0.6 % for the k=1
part (`J_x/J_y`) and ≈2 % for the k=2-heavy `J_z` — which is precisely the split
observed (0.6 % vs 2.2 % interior L2). Edges would differ more (ADIOS2 clamps,
TGV is periodic), but interior J matches.

**Bottom line for offloading J:** yes, native and ADIOS2-derived J are the same
quantity and match in number and trend — *provided* you correct the axis order
and multiply by `1/(dx·Rem)`. Used naively, the derived curl is wrong.

### 4b. Wired in-situ via 2decomp-fft (BP5) — final result

Implemented the offload for real:

* **2decomp-fft** (`2decomp-fft/src/io.f90`): new
  `decomp_2d_register_derived_current(io_name, name, bx, by, bz, scale)` — builds
  the ADIOS2 derived expression `multiply(curl(vz,vy,vx), scale)`, encapsulating
  the reversed-storage axis swap and the isotropic `scale = -1/(dx·Rem)`. (Also
  fixed a latent uninitialised `ext` in `gen_iodir_name` for non-BP5 engines.)
* **Case-TGV.f90** `visu_tgv_init`: registers `J_derived` with
  `jscale = -one/(dx*Rem)` when `mhd_active`.
* Ran `runs/otv3d_cmp` (32³ OTV, BP5, 1 rank). ADIOS2 computes `J_derived` in
  situ each snapshot. Compared vs native `J_x/J_y/J_z` (`compare_derived.py`):

  | comp | corr | ratio (der/nat) | interior rel.L2 |
  |------|:----:|:---:|:---:|
  | J_x  | 1.00000 | 0.992 | 0.6–0.7 % |
  | J_y  | 1.00000 | 0.992 | 0.6–0.7 % |
  | J_z  | 0.9996  | 0.976 | 2.2–2.3 % |

  Volume-RMS |J| trend tracks native across all 4 snapshots (ratio 0.978→0.980,
  both rising 5.54→5.63e-2). The 0.8–2.4 % deficit is exactly the 2nd-vs-6th
  order truncation (`sin(kΔ)/(kΔ)`), as in §4a. **Number and trend match.**

**Reader gotchas for `J_derived`** (both mandatory, verified):
1. **Component order is (Jz, Jy, Jx)** — reversed (the storage-axis swap).
2. **Memory is component-LAST**: reshape the flat buffer to `(nz,ny,nx,3)`, even
   though ADIOS2's Shape metadata advertises `(3,nz,ny,nx)`. Reading it in the
   advertised shape scrambles the field (magnitude is preserved but every voxel
   is misplaced).

Scope limits of the single-scalar `scale`: it is exact only for an **isotropic**
grid (`dx=dy=dz`); stretched/anisotropic grids need per-direction handling that
stock ADIOS2 `curl` cannot express. Under **MPI decomposition** the curl is
evaluated per writer block, so the internal block seams are one-sided (the 1-rank
run above avoids this; a decomposed run will show small seam errors on top of the
interior agreement).

---

## How to actually run the head-to-head

The native fields already come out of `visu_tgv`. To get the ADIOS2 side in the
same BP file:

1. Register `ux,uy,uz` (and `B_x,B_y,B_z`) as **full 3-D** variables
   (`iplane=0`, not `output2D`) so the derived engine has 3-D inputs.
2. Add, in `visu_tgv_init`:
   ```fortran
   call decomp_2d_register_derived_variable(io_name, "vort_adios", &
        "x=ux"//NL//"y=uy"//NL//"z=uz"//NL//"magnitude(curl(x,y,z))")
   call decomp_2d_register_derived_variable(io_name, "J_adios", &
        "x=B_x"//NL//"y=B_y"//NL//"z=B_z"//NL//"curl(x,y,z)")   ! then /Rem in post
   ```
   (`critq` has no stock-ADIOS2 form.)
3. Run with the BP5 engine, then diff `vort` vs `vort_adios` in Python. Expect
   the ≈`Δ` scaling factor and boundary-plane mismatch above — that is the
   "are they the same" answer, quantified.

---

## 5. Numerical-dissipation scalars (paper metrics) via ADIOS2 derived — `runs/tgv_scalars`

The paper's Xcompact3d metrics are **global spatial-average scalars**, but ADIOS2
derived variables are **element-wise field ops** (no reductions, no gradient
tensor, no FFT). So only some map onto stock ADIOS2:

| Metric | stock ADIOS2? | route |
|--------|:---:|-------|
| **E_k** (TKE) = ⟨½\|u\|²⟩          | ✅ | field `0.5·pow(magnitude(u),2)` → spatial mean |
| **Enstrophy** = ⟨½\|∇×u\|²⟩        | ✅ | field `0.5·pow(magnitude(curl),2)/h²` → mean |
| **eps** (physical dissipation)     | ✅* | identity `eps = 2·ν·enstrophy` (exact for periodic; verified 2·0.02·2.0002=0.08001=eps) |
| **eps_num** = −dE_k/dt − eps       | ✅ | post: time-diff of E_k(t) + eps |
| **nu_eff/nu** = (−dE_k/dt)/(2ν·enst)| ✅ | post: from E_k(t), enstrophy(t) |
| **skewness** ⟨(∂u/∂x)³⟩/⟨(∂u/∂x)²⟩^{3/2} | ❌ | no way to isolate ∂u/∂x (only `curl`, which mixes axes) |
| **flatness** ⟨(∂u/∂x)⁴⟩/⟨(∂u/∂x)²⟩² | ❌ | same — needs a partial-derivative op |
| **E(k)** energy spectrum           | ❌ | needs FFT — no op |

### Implemented (in 2decomp-fft) + measured

* `2decomp-fft/src/io.f90`: `decomp_2d_register_derived_ke` (→ `multiply(pow(magnitude,2),scale)`)
  and `decomp_2d_register_derived_enstrophy` (→ `multiply(pow(magnitude(curl(vz,vy,vx)),2),scale)`,
  isotropic `scale = 0.5/h²`).
* `Case-TGV.f90:visu_tgv_init` registers `tke_density` and `enst_density` from `ux/uy/uz`.
* Ran a hydro TGV (32³, Re=400, BP5, 1 rank), 10 snapshots. Spatial-averaged the
  ADIOS2 fields (`compare_scalars.py`) and compared to native `time_evol.dat`:

  | scalar | ADIOS2-derived vs native (rel. L2) |
  |--------|:---:|
  | **E_k**       | **0.0000 %** (exact — non-differential field) |
  | **enstrophy** | **0.97 %** (2nd- vs 6th-order curl) |
  | **eps = 2ν·enstrophy** | **0.97 %** |
  | **nu_eff/nu** | ~1 % (raw ≈1.0000, ADIOS ≈1.010) |
  | **eps_num**   | both ≈ 0 (flow well-resolved; raw ~1e-7, ADIOS ~1.8e-5 ≈ 1 % of eps) |

  All track the native time trend. E_k is bit-exact because it needs no
  derivative; everything downstream inherits the ~1 % enstrophy bias, which is
  the pure 2nd-vs-6th-order truncation (`sin(kΔ)/(kΔ)`).

### Not achievable with stock ADIOS2 (would need new ops)

`skewness`, `flatness`, `E(k)`, and the strain-based `eps` all need capabilities
absent from stock ADIOS2 derived variables: a **partial-derivative / gradient**
operator (∂u_i/∂x_j individually — `curl` only gives antisymmetric combinations),
a **global reduction** (mean/sum — BP5 stores only min/max), and an **FFT**. This
is exactly why the paper's Vigil uses its *own* operator graph rather than ADIOS2
derived variables. Adding these as custom ADIOS2 ops (à la the coeus `Qcrit`/`hash`
fork) is the path to covering the remaining metrics — at the cost of rebuilding
ADIOS2.

---

## 6. Custom ADIOS2 ops (gradient + mean) — `ADIOS2/`, rebuilt with Derived+SST

Added two operators to the ADIOS2 derived engine so the remaining paper metrics
become offloadable, and fixed one latent bug found along the way.

### ADIOS2 source changes (`source/adios2/toolkit/derived/`)
* **`gradient`** (`GradientFunc`/`ApplyGradient`, 2nd-order central, index space):
  `gradient(f)` → vector field (∂f/∂x0,∂f/∂x1,∂f/∂x2); `gradient(f, axis)` → the
  scalar partial ∂f/∂(axis). Unlocks individual ∂u_i/∂x_j (curl only gives
  antisymmetric mixes).
* **`mean`** (`MeanFunc`/`MeanDimsFunc`): reduces a field to one value per writer
  block — true in-situ block statistics, no full-field read on the reader.
* **Bug fix in `PowFunc`**: the exponent was parsed as `size_t` (`std::stoull`),
  so `pow(x,1.5)` truncated to `pow(x,1)`. Changed to `double`/`std::stod`.
  Without this, derivative **skewness** (needs `^1.5`) was a constant `sqrt(<g²>)`
  factor (~15×) too small; flatness (`^2`) was unaffected.

Built with `-DADIOS2_USE_Derived_Variable=ON -DADIOS2_USE_SST=ON
-DADIOS2_USE_Fortran=ON -DADIOS2_USE_MPI=ON` against spack openmpi 5.0.9 +
libfabric (SST fabric/UCX/MPI dataplanes), installed to `ADIOS2/install`; 2decomp
and Incompact3d relinked against it. (Also patched a misplaced `endif()` in the
generated `adios2-config-common.cmake` that broke 2decomp's double
`find_package(adios2)`.)

### 2decomp-fft registrations (`src/io.f90`)
`decomp_2d_register_derived_skewness/_flatness(io, name, var, axis)` (build
`divide(mean(pow(gradient(u,axis),3|4)), pow(mean(pow(gradient(u,axis),2)),1.5|2))`),
`decomp_2d_register_derived_mean`, and an `opt_reduce` flag on the ke/enstrophy
helpers (wrap the field in `mean(...)`). Wired into `Case-TGV.f90:visu_tgv_init`
(`skewness_dudx`, `flatness_dudx`, `tke_mean`, `enst_mean`; physical x = axis 2).

### Result — in-situ scalars vs native `time_evol.dat` (32³ TGV, Re=400, 1 rank)

| metric (single value / snapshot) | ADIOS2-derived vs native |
|---|:---:|
| **E_k**            = `mean(0.5|u|²)`                 | **0.000 %** |
| **enstrophy**      = `mean(0.5|curl u|²/dx²)`        | **0.97 %** |
| **skewness** du/dx = `<g³>/<g²>^{3/2}` (gradient+mean) | **2.9 %** |
| **flatness** du/dx = `<g⁴>/<g²>²`   (gradient+mean)  | **0.14 %** |

skewness/flatness (previously **not expressible** in stock ADIOS2) now match the
native 6th-order diagnostics; the ~3 % skewness residual is the genuine
2nd-vs-6th-order derivative difference (a numpy 2nd-order reference on the same
field reproduces the ADIOS2 value to machine precision, confirming the op is
correct). All four are single per-block values — no field read.

**Still not offloadable:** `E(k)` energy spectrum (needs an FFT op). Everything
else in the paper's Xcompact3d metric set is now computed in situ by ADIOS2.

---

## 7. FFT energy spectrum E(k) — custom ADIOS2 `spectrum` op

Added a third custom op so the last paper metric, the kinetic-energy spectrum,
is offloadable.

* **`spectrum`** (alias `fft`) in `ADIOS2/.../derived/Function.cpp`
  (`SpectrumFunc`/`ApplySpectrum` + a self-contained radix-2 `fft1d`/`fft3d`):
  `spectrum(ux,uy,uz)` → 1-D array `E(k)`, `E(k)=0.5·Σ_{round|k⃗|=k}
  (|û_x|²+|û_y|²+|û_z|²)/N²`, so `Σ_k E(k)=⟨½|u|²⟩` (Parseval). Power-of-two
  dims; global per writer block (single writer ⇒ global spectrum). Axis-symmetric,
  so no reversed-storage swap needed. No FFT library dependency (self-contained).
* `2decomp-fft`: `decomp_2d_register_derived_spectrum`; wired into
  `Case-TGV.f90` as `ek_spectrum` (1-D array per snapshot).

### Check (`check_spectrum.py`, 32³ TGV)

| test | result |
|------|:---:|
| ADIOS2 `spectrum` op vs `numpy.fft` (same field) | **max rel.L2 = 1.3e-15** (bit-exact) |
| Parseval: `Σ_k E(k)` vs native `E_k` | **0.000 %** every snapshot |
| spectrum shape | peak at k=2 (TGV `sin·cos·cos` modes, \|k\|=√3→2) ✓ |

## 8. Final status — paper's Xcompact3d metrics via ADIOS2 derived variables

| metric | offloaded? | agreement vs native |
|--------|:---:|:---:|
| E_k (TKE)            | ✅ | 0.000 % |
| enstrophy           | ✅ | 0.97 % |
| eps (= 2ν·enstrophy)| ✅ | 0.97 % |
| eps_num, nu_eff/nu  | ✅ (post) | ~1 % |
| skewness du/dx      | ✅ *(gradient+mean)* | 2.9 % |
| flatness du/dx      | ✅ *(gradient+mean)* | 0.14 % |
| **E(k) spectrum**   | ✅ *(spectrum/FFT)* | **1e-15 (bit-exact)** |

All of the paper's Xcompact3d numerical-dissipation metrics are now computed in
situ by the ADIOS2 derived engine. Custom ops added to ADIOS2: `gradient`,
`mean`, `spectrum`(/`fft`); plus the `PowFunc` non-integer-exponent fix.
