#!/usr/bin/env python3
"""Standalone ADIOS2 SST/BP5 consumer for the LAMMPS atom stream (Vigil render).

The render phase of the trigger-render-reason pipeline for the LAMMPS
velocity-Verlet-failure case. The modified `dump custom/adios` writes the
de-interleaved per-column variables `x y z vx vy vz` (and, with
COEUS_LAMMPS_DERIVED=1, the block-mean `derive/V2mean = mean(|v|^2) = 3*T*`).
The hermes `mean` trigger ships the super-heated window over SST; this reader
consumes it, renders each flagged step as a **particle scatter** (atoms colored
by speed |v|), and publishes the frame stats for the MCP server / agent.

Zero ParaView / Fides dependency: it opens the ADIOS2 stream directly, loops
BeginStep/Get(x,y,z,vx,vy,vz)/EndStep, computes the kinetic temperature
T* = <|v|^2>/3, counts non-finite / escaped atoms (the "Lost atoms" blow-up
signature), and rasterizes the scatter to a PNG with only numpy + the stdlib
(the adios2 python env usually has no matplotlib).

Usage (offline test on a BP5 file -- easiest, no SST/hermes/ParaView):
    python3 lammps_sst_reader.py --stream lammps.bp --engine BP5 \
        --png-dir frames --status-file lammps_status.json

Usage (live gated SST from the hermes engine):
    python3 lammps_sst_reader.py --stream lammps.bp --engine SST \
        --png-dir frames --status-file lammps_status.json

Usage (match the writer's XML engine exactly):
    python3 lammps_sst_reader.py --stream lammps.bp --config ../adios2_config.xml --io custom
"""
import argparse
import json
import math
import os
import sys

import numpy as np
import adios2.bindings as adios2

# equilibrium set point of the LJ case (velocity create 0.75); T* runs away from
# this when the timestep is too large.
T_STAR_SETPOINT = 0.75

# Default (absolute) speed color-scale max: ~7x the equilibrium rms speed
# sqrt(3*T*). An ABSOLUTE scale (not per-frame) is deliberate so a healthy frame
# reads uniformly cold-blue and a blow-up reads hot-red -- a per-frame auto-scale
# would paint the narrow equilibrium spread as a full blue->red rainbow and make
# a healthy frame look hot.
SPEED_CLAMP_DEFAULT = 7.0 * math.sqrt(3.0 * T_STAR_SETPOINT)


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--stream", default="lammps.bp",
                    help="ADIOS2 stream/file name the dump opened (default lammps.bp)")
    ap.add_argument("--config", default=None,
                    help="ADIOS2 XML config; engine taken from <io name=IONAME>")
    ap.add_argument("--io", default="custom",
                    help="IO name in the XML config (default 'custom', the dump's IO group)")
    ap.add_argument("--engine", default="SST",
                    help="engine when --config is not given (SST or BP5; default SST)")
    ap.add_argument("--timeout", type=float, default=120.0,
                    help="seconds to wait for each SST step (default 120)")
    ap.add_argument("--max-steps", type=int, default=0,
                    help="stop after this many steps (0 = until end of stream)")
    ap.add_argument("--png-dir", default=None,
                    help="render one atom-scatter PNG per step into this directory")
    ap.add_argument("--project", default="xy", choices=["xy", "xz", "yz"],
                    help="projection plane for the 2D scatter (default xy)")
    ap.add_argument("--img-size", type=int, default=700,
                    help="rendered PNG edge length in pixels (default 700)")
    ap.add_argument("--clamp", type=float, default=0.0,
                    help="speed color-scale max. 0 = absolute default (~7x the "
                         "T*=0.75 rms speed) so equilibrium reads cold-blue and a "
                         "blow-up reads hot-red; >0 = that absolute max; <0 = "
                         "per-frame 99.5th-percentile auto")
    ap.add_argument("--status-file", default=None,
                    help="write the latest received frame's stats to this JSON "
                         "(the MCP server / agent reads it to judge the blow-up)")
    args = ap.parse_args()

    if args.png_dir:
        os.makedirs(args.png_dir, exist_ok=True)

    if args.config:
        adios = adios2.ADIOS(args.config)
        io = adios.DeclareIO(args.io)
    else:
        adios = adios2.ADIOS()
        io = adios.DeclareIO(args.io)
        io.SetEngine(args.engine)

    print("[consumer] opening '%s' for read (engine=%s) ..."
          % (args.stream, args.config or args.engine), flush=True)
    engine = io.Open(args.stream, adios2.Mode.Read)
    print("[consumer] stream open", flush=True)

    ax0, ax1 = {"xy": (0, 1), "xz": (0, 2), "yz": (1, 2)}[args.project]

    n = 0
    while True:
        status = engine.BeginStep(adios2.StepMode.Read, args.timeout)
        if status == adios2.StepStatus.EndOfStream:
            print("[consumer] end of stream", flush=True)
            break
        if status == adios2.StepStatus.NotReady:
            print("[consumer] timed out waiting for a step; stopping", flush=True)
            break
        if status != adios2.StepStatus.OK:
            print("[consumer] step status %s; stopping" % status, flush=True)
            break

        step_val = _first_scalar_int(engine, io, ("step", "ntimestep", "timestep"))

        cols = {c: _get_array(engine, io, c) for c in ("x", "y", "z", "vx", "vy", "vz")}

        # in-situ derived kinetic temperature (== 3*T*) if the dump declared it
        v2mean = _get_scalar_double(engine, io, "derive/V2mean")

        # trigger scalars that travel with a hermes trigger-gated stream (absent
        # on a plain/ungated stream or an offline BP file)
        trig_fired = _get_scalar_int(engine, io, "vigil/trigger_fired")
        trig_stat = _get_scalar_double(engine, io, "vigil/trigger_stat")
        trig_fire_step = _get_scalar_int(engine, io, "vigil/trigger_fire_step")

        engine.EndStep()

        if any(cols[c] is None for c in ("vx", "vy", "vz")):
            print("[consumer] step %s has no vx/vy/vz columns; skipping "
                  "(needs the modified dump_custom_adios that de-interleaves "
                  "named columns)" % step_val, flush=True)
            n += 1
            continue

        vx, vy, vz = cols["vx"], cols["vy"], cols["vz"]
        speed2 = vx * vx + vy * vy + vz * vz
        finite = np.isfinite(speed2)
        n_atoms = int(speed2.size)
        n_bad = int((~finite).sum())
        if finite.any():
            speed = np.sqrt(speed2[finite])
            tstar = float(speed2[finite].mean() / 3.0)          # <|v|^2>/3
            smin, smax, smean = float(speed.min()), float(speed.max()), float(speed.mean())
        else:
            tstar = float("nan")
            smin = smax = smean = float("nan")

        ratio = tstar / T_STAR_SETPOINT if np.isfinite(tstar) else float("nan")
        msg = ("[consumer] recv #%d step=%s atoms=%d T*=%.4g (%.2gx setpoint) "
               "speed[min=%.3g max=%.3g]"
               % (n, step_val, n_atoms, tstar, ratio, smin, smax))
        if v2mean is not None:
            msg += " derive/V2mean=%.4g" % v2mean
        if trig_fired is not None:
            msg += (" [GATED fired=%s trigger_stat=%s fire_step=%s]"
                    % (trig_fired, _fmt(trig_stat), trig_fire_step))
        if n_bad:
            msg += "  *** %d NON-FINITE atoms (integration blow-up) ***" % n_bad
        print(msg, flush=True)

        image_path = None
        if args.png_dir:
            key = step_val if step_val is not None else n
            image_path = os.path.abspath(os.path.join(args.png_dir, "atoms_%06d.png" % key))
            a = cols[("x", "y", "z")[ax0]]
            b = cols[("x", "y", "z")[ax1]]
            # 0 -> absolute default scale; <0 -> per-frame percentile auto
            if args.clamp > 0:
                eff_clamp = args.clamp
            elif args.clamp < 0:
                eff_clamp = 0.0                      # _scatter_rgb: <=0 => percentile
            else:
                eff_clamp = SPEED_CLAMP_DEFAULT
            rgb = _scatter_rgb(a, b, np.sqrt(np.where(finite, speed2, np.nan)),
                               args.img_size, args.img_size, eff_clamp)
            _write_png(image_path, rgb)
            print("[consumer]   rendered %s" % image_path, flush=True)

        if args.status_file:
            snapshot = {
                "frame": n, "step": step_val, "n_atoms": n_atoms,
                "kinetic_temperature": tstar, "tstar_setpoint": T_STAR_SETPOINT,
                "tstar_ratio": ratio,
                "speed_min": smin, "speed_max": smax, "speed_mean": smean,
                "v2mean_derived": v2mean, "nonfinite_atoms": n_bad,
                "trigger_fired": trig_fired, "trigger_stat": trig_stat,
                "trigger_fire_step": trig_fire_step,
                "gated": trig_fired is not None,
                "projection": args.project,
                "image": image_path,
            }
            _atomic_write_json(args.status_file, snapshot)

        n += 1
        if args.max_steps and n >= args.max_steps:
            print("[consumer] reached --max-steps=%d" % args.max_steps, flush=True)
            break

    engine.Close()
    print("[consumer] done: %d step(s) received" % n, flush=True)
    return 0


# --------------------------------------------------------------------------- #
# rendering (numpy + stdlib only; the adios2 python env usually has no mpl/PIL)
# --------------------------------------------------------------------------- #

# sequential cold->hot colormap: slow/equilibrium atoms blue, exploding red.
_CMAP_STOPS = np.array([
    [0.10, 0.15, 0.45],   # 0.00 deep blue  (slow / cold ~ T* set point)
    [0.15, 0.55, 0.78],   # 0.25 cyan-blue
    [0.20, 0.72, 0.35],   # 0.50 green
    [0.96, 0.82, 0.15],   # 0.75 yellow
    [0.88, 0.16, 0.12],   # 1.00 red        (fast / hot ~ blowing up)
])
_BG = (18, 18, 24)


def _cmap(t):
    """t in [0,1] (N,) -> (N,3) float in [0,1]."""
    pos = np.linspace(0.0, 1.0, len(_CMAP_STOPS))
    return np.stack([np.interp(t, pos, _CMAP_STOPS[:, k]) for k in range(3)], axis=-1)


def _scatter_rgb(a, b, speed, W, H, clamp):
    """Rasterize a particle scatter (plane coords a,b colored by speed) to
    (H,W,3) uint8. Square aspect; hottest atoms drawn on top; y increases up.
    A handful of escaped atoms are clamped to the border instead of zooming the
    whole view out."""
    canvas = np.empty((H, W, 3), dtype=np.uint8)
    canvas[:] = _BG
    m = np.isfinite(a) & np.isfinite(b) & np.isfinite(speed)
    a, b, speed = a[m], b[m], speed[m]
    if a.size == 0:
        return canvas

    # square, slightly padded bounds centered on the bulk (robust to escapees)
    alo, ahi = np.percentile(a, [0.5, 99.5])
    blo, bhi = np.percentile(b, [0.5, 99.5])
    ca, cb = 0.5 * (alo + ahi), 0.5 * (blo + bhi)
    half = max(ahi - alo, bhi - blo, 1e-9) * 0.5 * 1.08
    alo, ahi, blo, bhi = ca - half, ca + half, cb - half, cb + half

    if clamp <= 0:
        clamp = float(np.percentile(speed, 99.5))
        if not np.isfinite(clamp) or clamp <= 0:
            clamp = float(np.max(speed)) or 1.0
    t = np.clip(speed / clamp, 0.0, 1.0)

    order = np.argsort(t)                       # ascending: hottest assigned last
    ix = np.clip(((a[order] - alo) / (ahi - alo) * (W - 1)), 0, W - 1).astype(np.intp)
    iy = np.clip(((b[order] - blo) / (bhi - blo) * (H - 1)), 0, H - 1).astype(np.intp)
    rgb = (_cmap(t[order]) * 255.0 + 0.5).astype(np.uint8)

    for dx in (-1, 0, 1):                       # 3x3 splat so single atoms show
        xx = np.clip(ix + dx, 0, W - 1)
        for dy in (-1, 0, 1):
            yy = np.clip(iy + dy, 0, H - 1)
            canvas[yy, xx] = rgb
    return canvas[::-1]                         # origin lower (y up)


def _write_png(path, rgb):
    """Write an (H,W,3) uint8 array as a PNG using only the stdlib."""
    import struct
    import zlib
    h, w, _ = rgb.shape
    raw = b"".join(b"\x00" + rgb[y].tobytes() for y in range(h))

    def _chunk(tag, data):
        return (struct.pack(">I", len(data)) + tag + data +
                struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF))

    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n")
        f.write(_chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)))
        f.write(_chunk(b"IDAT", zlib.compress(raw, 6)))
        f.write(_chunk(b"IEND", b""))


# --------------------------------------------------------------------------- #
# adios2 helpers
# --------------------------------------------------------------------------- #

def _fmt(x):
    return "%.3f" % x if isinstance(x, float) else str(x)


def _atomic_write_json(path, obj):
    tmp = path + ".tmp"
    with open(tmp, "w") as f:
        json.dump(obj, f)
    os.replace(tmp, path)


def _get_array(engine, io, name):
    """Read a 1-D per-atom variable as float64, or None if absent."""
    v = io.InquireVariable(name)
    if not v:
        return None
    try:
        shape = v.Shape()
        if not shape:
            return None
        n = int(shape[0])
        v.SetSelection([[0], [n]])
        buf = np.zeros(n, dtype=np.float64)
        engine.Get(v, buf, adios2.Mode.Sync)
        return buf
    except Exception as e:
        print("[consumer]   (could not read '%s': %s)" % (name, e), flush=True)
        return None


def _first_scalar_int(engine, io, names):
    for nm in names:
        val = _get_scalar_int(engine, io, nm)
        if val is not None:
            return val
    return None


# adios2 type string -> numpy dtype for scalar ints (ntimestep is uint64_t,
# vigil/trigger_* are int32_t; Get needs a matching-typed buffer).
_INT_DTYPE = {
    "int16_t": np.int16, "uint16_t": np.uint16,
    "int32_t": np.int32, "uint32_t": np.uint32,
    "int64_t": np.int64, "uint64_t": np.uint64,
}


def _get_scalar_int(engine, io, name):
    v = io.InquireVariable(name)
    if not v:
        return None
    try:
        dtype = _INT_DTYPE.get(v.Type(), np.int64)
        buf = np.zeros(1, dtype=dtype)
        engine.Get(v, buf, adios2.Mode.Sync)
        return int(buf[0])
    except Exception:
        return None


def _get_scalar_double(engine, io, name):
    v = io.InquireVariable(name)
    if not v:
        return None
    try:
        buf = np.zeros(1, dtype=np.float64)
        engine.Get(v, buf, adios2.Mode.Sync)
        return float(buf[0])
    except Exception:
        return None


if __name__ == "__main__":
    sys.exit(main())
