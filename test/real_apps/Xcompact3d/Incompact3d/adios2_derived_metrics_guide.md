# Offloading Xcompact3d numerical-dissipation metrics to ADIOS2 derived variables

This guide documents how the paper's Xcompact3d diagnostics — **E_k, enstrophy,
eps, eps_num, nu_eff/nu, velocity-derivative skewness & flatness, and the energy
spectrum E(k)** — are computed **in situ by the ADIOS2 derived-variable engine**
instead of (or alongside) the native Fortran code, plus the MHD current density
**J = ∇×B/Rem**. It covers the architecture, the custom ADIOS2 operators we
added, the 2decomp-fft registration API, how to build and run, and the measured
agreement against the native 6th-order diagnostics.

- **What runs the math:** a locally modified **ADIOS2 2.11.0** (`ADIOS2/`) with
  `-DADIOS2_USE_Derived_Variable=ON`, computed during `EndStep` of the BP5 writer.
- **Where it is wired:** `2decomp-fft/src/io.f90` (registration helpers) and
  `src/Case-TGV.f90` (`visu_tgv_init`).
- **Measured results:** Section 7. Reproduce with `runs/tgv_scalars/` and
  `runs/otv3d_cmp/`.

---

## 1. How ADIOS2 derived variables work

A *derived variable* is a named expression over primary variables that ADIOS2
evaluates itself, on the writer, when a step ends — before data leaves the node.
You declare it once at init:

```fortran
call adios2_define_derived_variable(handle, io, name, expression, type, ierr)
```

- **Expression language** (`ADIOS2/source/adios2/toolkit/derived/parser/`):
  alias lines `name = varname`, then one expression. Operators are **function
  calls** — `add/subtract/multiply/divide`, `pow`, `sqrt`, `sin/cos/…`,
  `magnitude`, `cross`, `curl` (stock) — plus our additions `gradient`, `mean`,
  `spectrum`. **There are no infix operators** (`*`,`/` are rejected) and **no
  `E` exponent notation** in numeric literals (use fixed-point, e.g. `0.5`).
- **Type:** `StoreData` (full field written) or `StatsOnly` (min/max metadata).
- **Engine:** BP5 only; requires the Derived_Variable build option.
- **Scope:** evaluated **per writer block**. With a single writer the block is
  the whole domain (global result); under MPI decomposition each rank produces a
  per-block result (see caveats, Section 8).

The 2decomp-fft IO layer wraps this in `decomp_2d_register_derived_variable`
(and the metric-specific helpers below).

---

## 2. Two coordinate/scaling facts you must respect

Everything below hinges on two properties of the ADIOS2 stencil operators
(`curl`, `gradient`):

1. **Reversed storage (z, y, x).** 2decomp writes 3-D fields with physical **x
   as the fastest (last) array axis**. ADIOS2's `curl`/`gradient` hard-assume
   `axis0=x, axis1=y, axis2=z`. Consequences:
   - `curl` must be fed **swapped**: `curl(vz, vy, vx)` reproduces the true
     ∇× (verified: corr 1.0000).
   - a partial derivative ∂/∂x is `gradient(u, 2)` (physical x = array axis 2).
   - `magnitude`, `mean`, `spectrum` are axis-symmetric → **no swap needed**.
2. **Index-space differentiation (grid spacing = 1).** `curl`/`gradient` use a
   2nd-order central difference divided by the *index* gap, not physical `Δx`.
   So results are a factor `Δ` off:
   - vorticity/current: multiply by `1/Δ` (isotropic grid).
   - enstrophy density `0.5|∇×u|²`: multiply by `0.5/Δ²`.
   - **skewness/flatness are dimensionless ratios → the `Δ` cancels, no
     correction.**

Both are isotropic-grid assumptions (`Δ = dx = dy = dz`); stretched grids need
per-direction handling that stock `curl`/`gradient` cannot express.

---

## 3. The metrics and their ADIOS2 expressions

Grid used below: `[0,2π]³`, `32³`, so `Δ = 2π/32 = 0.19635`. `ν = 1/Re`,
`Rem` = magnetic Reynolds number.

| metric | ADIOS2 expression (aliases omitted) | notes |
|--------|--------------------------------------|-------|
| **TKE density** `0.5\|u\|²` | `multiply(pow(magnitude(ux,uy,uz),2), 0.5)` | exact (no derivative) |
| **E_k** (scalar) | `mean( … TKE density … )` | `mean` op |
| **enstrophy density** `0.5\|∇×u\|²` | `multiply(pow(magnitude(curl(uz,uy,ux)),2), 0.5/Δ²)` | swap + `1/Δ²` |
| **enstrophy** (scalar) | `mean( … enstrophy density … )` | `mean` op |
| **eps** (dissipation) | `2·ν·enstrophy` (reader) | identity, exact for periodic |
| **eps_num**, **nu_eff/nu** | from `E_k(t)`, `enstrophy` (reader) | time-diff of E_k |
| **skewness** `⟨g³⟩/⟨g²⟩^{3/2}`, `g=∂u/∂x` | `divide(mean(pow(gradient(u,2),3)), pow(mean(pow(gradient(u,2),2)),1.5))` | `gradient`+`mean` |
| **flatness** `⟨g⁴⟩/⟨g²⟩²` | `divide(mean(pow(gradient(u,2),4)), pow(mean(pow(gradient(u,2),2)),2))` | `gradient`+`mean` |
| **E(k)** spectrum | `spectrum(ux,uy,uz)` | 3-D FFT + shell binning |
| **J** = ∇×B/Rem (MHD) | `multiply(curl(Bz,By,Bx), -1/(Δ·Rem))` | swap; comp order (Jz,Jy,Jx) |

---

## 4. Custom ADIOS2 operators added (`ADIOS2/source/adios2/toolkit/derived/`)

Stock ADIOS2 has only element-wise ops + `curl`/`magnitude`/`cross`. To cover
the paper metrics we added three operators and fixed one bug. Each op needs an
entry in the enum (`Expression.h`), three maps (`Expression.cpp`: `op_property`,
`string_to_op`, `OpFunctions`), a compute function + a dims function
(`Function.{h,cpp}`).

### 4.1 `gradient` — partial derivatives
`GradientFunc` / `ApplyGradient` (2nd-order central, index space, one-sided edges):
- `gradient(f)` → vector field `(∂f/∂x0, ∂f/∂x1, ∂f/∂x2)` (shape `(d0,d1,d2,3)`).
- `gradient(f, axis)` → scalar field `∂f/∂(axis)` (`axis` a `0/1/2` constant).

Unlocks individual ∂u_i/∂x_j (the full velocity-gradient tensor); `curl` alone
only gives antisymmetric combinations.

### 4.2 `mean` — reduction to a per-block scalar
`MeanFunc` / `MeanDimsFunc`: reduces a field to one value (`Count = {1}`) using a
`double` accumulator. This is the "block statistic" that lets a scalar diagnostic
be emitted **without the reader touching the full field**.

### 4.3 `spectrum` (alias `fft`) — energy spectrum via 3-D FFT
`SpectrumFunc` / `ApplySpectrum` with a self-contained radix-2 `fft1d`/`fft3d`
(no FFTW dependency):
```
spectrum(ux,uy,uz) → 1-D array E(k),
E(k) = 0.5 · Σ_{round|k⃗|=k} (|û_x|²+|û_y|²+|û_z|²) / N²   (Parseval: Σ_k E(k)=⟨½|u|²⟩)
```
Requires power-of-two dims; global per writer block.

### 4.4 Bug fix in `PowFunc`
The exponent was parsed as `size_t` (`std::stoull`), so `pow(x,1.5)` silently
truncated to `pow(x,1)` — which made **skewness** (needs `^1.5`) a constant
`√⟨g²⟩ ≈ 15×` too small. Fixed to `double`/`std::stod`. (`flatness`, `^2`, was
unaffected — that's how the bug was spotted.)

Files touched: `Expression.h`, `Expression.cpp`, `Function.h`, `Function.cpp`.

---

## 5. 2decomp-fft registration API (`2decomp-fft/src/io.f90`)

Thin Fortran wrappers that build the expression strings (handling the swap, the
scale, and fixed-point formatting) and call
`decomp_2d_register_derived_variable`. All are `public` from `decomp_2d_io`.

```fortran
! generic
call decomp_2d_register_derived_variable(io_name, name, expression [, opt_store_data])

! metric-specific
call decomp_2d_register_derived_current  (io, name, bx, by, bz, scale [,...])          ! J = scale*curl(B)
call decomp_2d_register_derived_ke       (io, name, ux, uy, uz, scale [,..., opt_reduce])   ! scale*|u|^2  (mean if opt_reduce)
call decomp_2d_register_derived_enstrophy(io, name, ux, uy, uz, scale [,..., opt_reduce])   ! scale*|curl u|^2
call decomp_2d_register_derived_mean     (io, name, var [,...])                          ! mean(var)
call decomp_2d_register_derived_skewness (io, name, var, axis [,...])                    ! <g^3>/<g^2>^1.5
call decomp_2d_register_derived_flatness (io, name, var, axis [,...])                    ! <g^4>/<g^2>^2
call decomp_2d_register_derived_spectrum (io, name, ux, uy, uz [,...])                   ! E(k)
```

Notes: the current/enstrophy helpers embed the reversed-storage **swap** and the
isotropic **scale**; `opt_reduce=.true.` wraps a density field in `mean(...)` for
a scalar; `axis` for skewness/flatness is the array axis of the physical
direction (physical x = 2).

---

## 6. Where it is wired (`src/Case-TGV.f90`, `visu_tgv_init`)

```fortran
integer, parameter :: axis_x = 2                 ! physical x = array axis 2 (reversed storage)

! field densities
call decomp_2d_register_derived_ke       (io_name, "tke_density",  "ux","uy","uz", half)
call decomp_2d_register_derived_enstrophy(io_name, "enst_density", "ux","uy","uz", half/(dx*dx))

! in-situ scalar reductions (single value per snapshot, no field read)
call decomp_2d_register_derived_ke       (io_name, "tke_mean",  "ux","uy","uz", half,          opt_reduce=.true.)
call decomp_2d_register_derived_enstrophy(io_name, "enst_mean", "ux","uy","uz", half/(dx*dx),  opt_reduce=.true.)

! velocity-derivative skewness / flatness of du/dx
call decomp_2d_register_derived_skewness (io_name, "skewness_dudx", "ux", axis_x)
call decomp_2d_register_derived_flatness (io_name, "flatness_dudx", "ux", axis_x)

! energy spectrum E(k)
call decomp_2d_register_derived_spectrum (io_name, "ek_spectrum",  "ux","uy","uz")

! MHD current density (mhd_active branch)
call decomp_2d_register_derived_current  (io_name, "J_derived", "B_x","B_y","B_z", -one/(dx*Rem))
```

The primary inputs (`ux,uy,uz`, `B_x/y/z`) are written full-3-D by the normal
snapshot path, so the derived engine has them each `EndStep`.

---

## 7. Build & run

### 7.1 Environment (spack)
```bash
source /home/hxu40/spack/share/spack/setup-env.sh
spack load openmpi@5.0.9 /ruzpfyp          # MPI + compilers (mpicc/mpicxx/mpif90)
export CC=mpicc CXX=mpicxx FC=mpif90
```

### 7.2 Build the modified ADIOS2 (Derived + SST + Fortran + MPI)
```bash
LIBFABRIC=$(spack location -i libfabric)
cmake -S ADIOS2 -B ADIOS2/build -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=ADIOS2/install \
  -DCMAKE_PREFIX_PATH="$LIBFABRIC" \
  -DADIOS2_USE_Derived_Variable=ON \
  -DADIOS2_USE_SST=ON -DADIOS2_USE_Fortran=ON -DADIOS2_USE_MPI=ON \
  -DADIOS2_USE_Python=OFF -DADIOS2_USE_HDF5=OFF \
  -DBUILD_TESTING=OFF -DADIOS2_BUILD_EXAMPLES=OFF \
  -DCMAKE_C_COMPILER=mpicc -DCMAKE_CXX_COMPILER=mpicxx -DCMAKE_Fortran_COMPILER=mpif90
cmake --build ADIOS2/build -j8
cmake --install ADIOS2/build
```
(One-time: the generated `ADIOS2/install/lib/cmake/adios2/adios2-config-common.cmake`
has a misplaced `endif()`; guard the deprecated block by changing
`if(${CMAKE_FIND_PACKAGE_NAME}_CXX_FOUND)` to
`if(${CMAKE_FIND_PACKAGE_NAME}_CXX_FOUND AND NOT TARGET adios2::cxx11)` so
2decomp's double `find_package(adios2)` doesn't error.)

### 7.3 Build 2decomp-fft, then Incompact3d, against it
```bash
ADIOS2_DIR=$PWD/ADIOS2/install/lib/cmake/adios2

cmake -S 2decomp-fft -B 2decomp-fft/build -DCMAKE_BUILD_TYPE=Release \
  -DIO_BACKEND=adios2 -Dadios2_DIR=$ADIOS2_DIR \
  -DCMAKE_INSTALL_PREFIX=2decomp-fft/opt -DBUILD_TESTING=OFF
cmake --build 2decomp-fft/build -j4 && cmake --install 2decomp-fft/build

cmake -S . -B ./build -DIO_BACKEND=adios2 -Dadios2_DIR=$ADIOS2_DIR \
  -Ddecomp2d_DIR=$PWD/2decomp-fft/opt/lib/decomp2d
cmake --build ./build -j4
```

### 7.4 Run (needs an `adios2_config.xml` with a **BP5** engine in the run dir)
```bash
export LD_LIBRARY_PATH=$PWD/ADIOS2/install/lib:$LD_LIBRARY_PATH   # use the modified libs
cd runs/tgv_scalars
mpirun -n 1 ../../build/bin/xcompact3d input.i3d     # 1 rank => global reductions
```
This writes `data.bp5/` (derived variables) and `time_evol.dat` (native scalars).

---

## 8. Results (32³ TGV, Re=400, 10 snapshots; MHD OTV 32³, Rem=50, 4 snapshots)

### 8.1 Scalar metrics — ADIOS2-derived vs native `time_evol.dat`

| metric | how (ADIOS2) | rel. L2 vs native |
|--------|--------------|:---:|
| **E_k** (TKE)        | `mean(0.5\|u\|²)`                    | **0.000 %** (exact) |
| **enstrophy**        | `mean(0.5\|curl u\|²/Δ²)`            | **0.97 %** |
| **eps** = 2ν·enstrophy | identity                          | **0.97 %** |
| **nu_eff/nu**, **eps_num** | post from E_k(t), enstrophy   | ~1 % |
| **skewness** du/dx   | `gradient`+`mean`                    | **2.9 %** |
| **flatness** du/dx   | `gradient`+`mean`                    | **0.14 %** |
| **E(k)** spectrum    | `spectrum`/`fft`                     | **1.3e-15** (vs numpy.fft) |

Example values (last snapshot, t=0.199): E_k 0.124625 (native 0.124625),
enstrophy 0.371735 (0.375431), skewness −0.10287 (−0.10599), flatness 3.38859
(3.39424). The ~3 % skewness gap is the genuine 2nd-vs-6th-order derivative
difference (a numpy 2nd-order reference on the same field reproduces the ADIOS2
value to machine precision).

Sample of the in-situ scalar time series (`data.bp5`):
```
snap  E_k(tke_mean)  enstrophy   skewness   flatness
   1    0.12496251   0.371275   -0.010300   3.37108
   5    0.12481258   0.371184   -0.051468   3.37533
  10    0.12462504   0.371735   -0.102873   3.38859
```

### 8.2 Energy spectrum E(k)
- op vs `numpy.fft` (same field): **max rel. L2 = 1.3e-15** (bit-exact)
- Parseval `Σ_k E(k)` vs native E_k: **0.000 %** every snapshot
- shape: peak at k=2 (TGV `sin·cos·cos` modes, |k|=√3→2), decaying ~10⁻³/shell
```
E(2)=1.243e-01   E(3)=3.094e-04   E(4)=3.821e-07   E(5)=1.886e-09  ...
```

### 8.3 MHD current density J (case `runs/otv3d_cmp`)
`J_derived` = `multiply(curl(Bz,By,Bx), -1/(Δ·Rem))`, stored 4-D `{3,32,32,32}`,
component order **(Jz, Jy, Jx)**, memory **component-last** (reshape flat buffer
to `(nz,ny,nx,3)`).

| component | corr with native | ratio | interior rel.L2 |
|-----------|:---:|:---:|:---:|
| J_x | 1.00000 | 0.992 | 0.6–0.7 % |
| J_y | 1.00000 | 0.992 | 0.6–0.7 % |
| J_z | 0.9996  | 0.976 | 2.2–2.3 % |

---

## 9. Verification / post-processing

The `data.bp5` folders are read with `bpls` or the Python `adios2` reader (any
2.11 build reads standard BP5 — the modified libs are only needed to *write*).

```bash
runs/tgv_scalars/compare_newops.py    # scalars vs time_evol.dat
runs/tgv_scalars/check_spectrum.py    # E(k) vs numpy.fft + Parseval
runs/otv3d_cmp/compare_derived.py     # J_derived vs native J
```

---

## 10. Caveats & limitations

- **Reversed storage & index-space scaling** (Section 2): baked into the 2decomp
  helpers for the isotropic TGV/OTV grid; **not valid on stretched grids**.
- **`mean`/`spectrum` are per writer block.** Single writer ⇒ exact global value.
  Under MPI decomposition they are per-block; a global reduction needs a
  size-weighted (mean) / summed (spectrum) combine on the reader side.
- **`spectrum` needs power-of-two dims** and cubic domains for the current
  binning.
- **skewness is discretization-sensitive** (odd, near-zero, high-k dominated):
  2nd-order differencing gives ~3 %; flatness/E_k/spectrum are far tighter.
- **Expression-language limits:** no infix operators (use `multiply`/`divide`),
  no `E`-exponent literals (fixed-point only; Fortran side formats with `F30.16`),
  and non-integer `pow` exponents require the `PowFunc` fix in Section 4.4.

---

## 11. File index

| file | role |
|------|------|
| `ADIOS2/source/adios2/toolkit/derived/Function.{h,cpp}` | `gradient`,`mean`,`spectrum` impls + `pow` fix |
| `ADIOS2/source/adios2/toolkit/derived/Expression.{h,cpp}` | op enum + registration maps |
| `2decomp-fft/src/io.f90` | `decomp_2d_register_derived_*` helpers |
| `src/Case-TGV.f90` | `visu_tgv_init` registrations |
| `runs/tgv_scalars/` | hydro TGV case + comparison scripts |
| `runs/otv3d_cmp/` | MHD OTV case (J) + comparison scripts |
| `derived_comparison.md` | detailed development log / findings |
