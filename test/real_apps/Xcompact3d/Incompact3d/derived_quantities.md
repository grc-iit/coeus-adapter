# Derived Quantities in the TGV Case (Xcompact3d)

How each post-processed / derived field written by the Taylor–Green Vortex case is
computed, with source references. All line numbers refer to `src/Case-TGV.f90`
and `src/mhd.f90`.

The primary (solved) fields `ux`, `uy`, `uz` (velocity) and `pp` (pressure) are
written directly by the core solver. The quantities below are **derived** —
computed from velocity (or magnetic) gradients at output time.

---

## Velocity gradient tensor

All velocity-based derived quantities start from the nine components of the
velocity gradient tensor, obtained with the 6th-order compact first-derivative
operators (`derx`/`dery`/`derz`) after transposing between x/y/z pencils
(`Case-TGV.f90:595-618`):

```
du/dx = ta1   du/dy = td1   du/dz = tg1
dv/dx = tb1   dv/dy = te1   dv/dz = th1
dw/dx = tc1   dw/dy = tf1   dw/dz = ti1
```

---

## 1. `vort` — Vorticity magnitude

The magnitude of the vorticity vector **ω = ∇ × u**.

$$
\lvert\boldsymbol{\omega}\rvert =
\sqrt{
\left(\frac{\partial w}{\partial y}-\frac{\partial v}{\partial z}\right)^2
+\left(\frac{\partial u}{\partial z}-\frac{\partial w}{\partial x}\right)^2
+\left(\frac{\partial v}{\partial x}-\frac{\partial u}{\partial y}\right)^2
}
$$

Source (`Case-TGV.f90:620-624`):

```fortran
di1(:,:,:) = sqrt(  (tf1 - th1)**2 &   ! (dw/dy - dv/dz)
                  + (tg1 - tc1)**2 &   ! (du/dz - dw/dx)
                  + (tb1 - td1)**2 )   ! (dv/dx - du/dy)
call write_field(di1, ".", "vort", num, flush=.true.)
```

---

## 2. `critq` — Q-criterion

The second invariant of the velocity gradient tensor, used for vortex
identification (`Q > 0` marks rotation-dominated regions).

$$
Q = -\tfrac{1}{2}\left(
\left(\tfrac{\partial u}{\partial x}\right)^2 +
\left(\tfrac{\partial v}{\partial y}\right)^2 +
\left(\tfrac{\partial w}{\partial z}\right)^2 \right)
- \frac{\partial u}{\partial y}\frac{\partial v}{\partial x}
- \frac{\partial u}{\partial z}\frac{\partial w}{\partial x}
- \frac{\partial v}{\partial z}\frac{\partial w}{\partial y}
$$

Source (`Case-TGV.f90:626-632`):

```fortran
di1(:,:,:) = - zpfive*(ta1**2 + te1**2 + ti1**2) &  ! diagonal terms
             - td1*tb1 &                            ! (du/dy)(dv/dx)
             - tg1*tc1 &                            ! (du/dz)(dw/dx)
             - th1*tf1                              ! (dv/dz)(dw/dy)
call write_field(di1, ".", "critq", num, flush=.true.)
```

---

# MHD fields (only written when `mhd_active = .true.`)
#
Registered in `visu_tgv_init` (`Case-TGV.f90:547-553`) but written only if MHD is
enabled (`Case-TGV.f90:634-644`). In the standard TGV runs MHD is off, so no
`B_*`/`J_*` binaries are produced. Formulas live in `src/mhd.f90`.

### 3. `B_x` / `B_y` / `B_z` — Magnetic field

`B` (`Bm`) is a **state variable**, not recomputed at output time. It is
allocated in `mhd_init` (`mhd.f90:73`) with initial condition `Bm = (0,1,0)`
plus a mean field `Bmean` (`mhd.f90:84-95`). Its evolution depends on
`mhd_equation`:

- **`induction`** — transported by the induction equation

$$
\frac{\partial \mathbf{B}}{\partial t}
= \nabla\times(\mathbf{u}\times\mathbf{B})
+ \frac{1}{Re_m}\nabla^2\mathbf{B}
$$

  RHS assembled in skew-symmetric form in `mhd_rhs_eq` (`mhd.f90:411`), resistive
  term scaled by `1/Rem` (`mhd.f90:499`); time-marched by `int_time_magnet`
  (`mhd.f90:123`) using the same AB/RK scheme as the flow.

- **`potential`** — quasi-static (low-`Rem`) limit: `B` is the imposed field and
  is not transported.

At output, the writer simply dumps the current `Bm` components.

### 4. `J_x` / `J_y` / `J_z` — Current density

`J` (`Je`) **is** derived, recomputed from `B` and `u`
(`momentum_forcing_mhd`, `mhd.f90:197-203`; output recompute at
`Case-TGV.f90:639`):

- **`induction`** — Ampère's law

$$
\mathbf{J} = \frac{1}{Re_m}\,\nabla\times\mathbf{B}
$$

  via `Je = del_cross_prod(Bm+Bmean)/Rem` (`mhd.f90:198`). `del_cross_prod`
  (`mhd.f90:232`) computes the curl using 6th-order compact gradients
  (`grad_vmesh`, `mhd.f90:267`):

$$
(\nabla\times\mathbf B)_x=\partial_y B_z-\partial_z B_y,\quad
(\nabla\times\mathbf B)_y=\partial_z B_x-\partial_x B_z,\quad
(\nabla\times\mathbf B)_z=\partial_x B_y-\partial_y B_x
$$

- **`potential`** — from an electric-potential Poisson solve
  (`solve_mhd_potential_poisson`, `mhd.f90:311`):

$$
\mathbf{J} = -\nabla\phi + (\mathbf{u}\times\mathbf{B}),
\qquad \nabla^2\phi = \nabla\cdot(\mathbf{u}\times\mathbf{B})
$$

  where `u×B` is formed pointwise (`mhd.f90:344`), its divergence is the Poisson
  RHS (`mhd.f90:350`), solved with the same FFT `poisson` routine as pressure
  (`mhd.f90:356`), and `Je = -∇φ + u×B` (`mhd.f90:367`).

### Coupling back to momentum

Both fields re-enter the momentum equation as the Lorentz force, scaled by the
Stuart number `N` (`mhd.f90:209-213`):

$$
\mathbf{f} = N\,\bigl(\mathbf{J}\times(\mathbf{B}+\mathbf{B}_{mean})\bigr)
$$

---

## Summary

| Field | Type | Definition |
|-------|------|------------|
| `ux`, `uy`, `uz` | Primary (solved) | Velocity components |
| `pp` | Primary (solved) | Pressure |
| `vort` | Derived | Vorticity magnitude \|∇×u\| |
| `critq` | Derived | Q-criterion (2nd invariant of ∇u) |
| `B_x/y/z` | State (MHD only) | Magnetic field, transported (induction) or imposed (potential) |
| `J_x/y/z` | Derived (MHD only) | Current density: ∇×B/Rem, or −∇φ + u×B |

Key nondimensional parameters (read in `mhd_init`): `Rem` (magnetic Reynolds
number / resistivity) and `stuart` (Lorentz-force strength).
