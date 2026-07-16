#!/usr/bin/env python3
"""Standalone ADIOS2 SST/BP5 consumer for the 2D LBM-CFD vorticity stream.

Zero coeus dependency: it opens the ADIOS2 stream written by `lbmcfd --adios2`,
loops BeginStep/Get(vorticity)/EndStep, and reports step / min / max / any
non-finite cells (the instability signature). If `--png-dir` is given it also
renders one vorticity image per received step. Optionally reports the in-situ
derived instability signal `derive/VarVort` = variance(vorticity) when present.

Usage (SST, no config needed):
    python3 lbm_sst_reader.py --stream lbmcfd.bp --engine SST

Usage (match the writer's XML engine exactly):
    python3 lbm_sst_reader.py --stream lbmcfd.bp --config ../adios2.xml

Usage (inspect a BP5 file):
    python3 lbm_sst_reader.py --stream lbmcfd.bp --engine BP5
"""
import argparse
import json
import os
import sys

import numpy as np
import adios2.bindings as adios2


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--stream", default="lbmcfd.bp",
                    help="ADIOS2 stream/file name the writer opened (default lbmcfd.bp)")
    ap.add_argument("--config", default=None,
                    help="ADIOS2 XML config; engine taken from <io name=IONAME>")
    ap.add_argument("--io", default="SimulationOutput",
                    help="IO name in the XML config (default SimulationOutput)")
    ap.add_argument("--engine", default="SST",
                    help="engine when --config is not given (SST or BP5; default SST)")
    ap.add_argument("--timeout", type=float, default=120.0,
                    help="seconds to wait for each SST step (default 120)")
    ap.add_argument("--max-steps", type=int, default=0,
                    help="stop after this many steps (0 = until end of stream)")
    ap.add_argument("--png-dir", default=None,
                    help="render one vorticity PNG per step into this directory")
    ap.add_argument("--clamp", type=float, default=0.0,
                    help="+/- vorticity clamp for the PNG color scale; 0 = auto "
                         "(per-frame 99.5th percentile, which reveals the "
                         "grid-scale instability speckle instead of letting a "
                         "few exploding cells saturate the image)")
    ap.add_argument("--status-file", default=None,
                    help="write the latest received frame's stats to this JSON "
                         "(the MCP server / agent reads it to judge chaos)")
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

        step_val = _get_scalar_int(engine, io, "step")
        time_val = _get_scalar_double(engine, io, "time")
        stable_val = _get_scalar_int(engine, io, "stable")

        vvort = io.InquireVariable("vorticity")
        if not vvort:
            print("[consumer] no 'vorticity' variable this step; skipping", flush=True)
            engine.EndStep()
            continue
        shape = vvort.Shape()                 # [total_y, total_x]
        vvort.SetSelection([[0, 0], shape])
        data = np.zeros(shape, dtype=np.float64)
        engine.Get(vvort, data, adios2.Mode.Sync)

        var_vort = _get_scalar_double(engine, io, "derive/VarVort")

        # Trigger scalars that travel with a hermes trigger-gated stream (the
        # engine writes these on every flagged step; absent on a plain stream).
        trig_fired = _get_scalar_int(engine, io, "vigil/trigger_fired")
        trig_stat = _get_scalar_double(engine, io, "vigil/trigger_stat")
        trig_fire_step = _get_scalar_int(engine, io, "vigil/trigger_fire_step")

        engine.EndStep()

        finite = np.isfinite(data)
        n_bad = int((~finite).sum())
        if finite.any():
            vmin = float(data[finite].min())
            vmax = float(data[finite].max())
        else:
            vmin = vmax = float("nan")

        msg = ("[consumer] recv #%d step=%s time=%s stable=%s shape=%s "
               "vort[min=%.4g max=%.4g]"
               % (n, step_val, _fmt(time_val), stable_val, tuple(shape), vmin, vmax))
        if var_vort is not None:
            msg += " var(vort)_blk0=%.4g" % var_vort
        if trig_fired is not None:
            msg += (" [GATED fired=%s trigger_stat=%s fire_step=%s]"
                    % (trig_fired, _fmt(trig_stat), trig_fire_step))
        if n_bad:
            msg += "  *** %d NON-FINITE cells (instability) ***" % n_bad
        print(msg, flush=True)

        if args.status_file:
            # Latest-frame snapshot the MCP server / agent reads to judge chaos.
            status = {
                "frame": n, "step": step_val, "time": time_val,
                "stable": stable_val, "shape": list(map(int, shape)),
                "vort_min": vmin, "vort_max": vmax,
                "vort_absmax": max(abs(vmin), abs(vmax)),
                "nonfinite_cells": n_bad,
                "var_vort": var_vort, "trigger_fired": trig_fired,
                "trigger_stat": trig_stat, "trigger_fire_step": trig_fire_step,
                "gated": trig_fired is not None,
            }
            tmp = args.status_file + ".tmp"
            with open(tmp, "w") as f:
                json.dump(status, f)
            os.replace(tmp, args.status_file)  # atomic

        if args.png_dir:
            key = step_val if step_val is not None else n
            out = os.path.join(args.png_dir, "vort_%05d.png" % key)
            _write_png(out, _colorize(data, args.clamp))
            if args.status_file:
                _touch_status_image(args.status_file, out)
            print("[consumer]   rendered %s" % out, flush=True)

        n += 1
        if args.max_steps and n >= args.max_steps:
            print("[consumer] reached --max-steps=%d" % args.max_steps, flush=True)
            break

    engine.Close()
    print("[consumer] done: %d step(s) received" % n, flush=True)
    return 0


def _fmt(x):
    return "%.3f" % x if isinstance(x, float) else str(x)


def _colorize(data, clamp):
    """Vorticity -> (H,W,3) uint8 via a diverging blue/white/red map.

    clamp<=0 auto-scales to the frame's 99.5th percentile of |vorticity|. That
    matters for this case: once the scheme diverges a handful of cells reach
    ~1e6 while 99% stay O(1), so a max-based scale renders an almost-blank
    image and hides the grid-scale speckle that identifies the instability.
    Returned rows are flipped so y increases upward (origin='lower').
    """
    finite = np.isfinite(data)
    if clamp <= 0:
        a = np.abs(data[finite]) if finite.any() else np.array([1.0])
        clamp = float(np.percentile(a, 99.5)) if a.size else 1.0
        if not np.isfinite(clamp) or clamp <= 0:
            clamp = 1.0
    x = np.clip(np.nan_to_num(data, nan=0.0, posinf=clamp, neginf=-clamp),
                -clamp, clamp) / clamp                       # -> [-1, 1]
    # Gamma-boost the magnitude: without it the near-zero bulk renders flat
    # white and the instability speckle is barely visible.
    x = np.sign(x) * np.abs(x) ** 0.45
    r = np.where(x >= 0, 1.0, 1.0 + x)
    g = 1.0 - np.abs(x)
    b = np.where(x <= 0, 1.0, 1.0 - x)
    rgb = (np.stack([r, g, b], axis=-1) * 255.0).astype(np.uint8)
    rgb = rgb[::-1]
    return np.repeat(np.repeat(rgb, 2, axis=0), 2, axis=1)  # 2x for legibility


def _write_png(path, rgb):
    """Write an (H,W,3) uint8 array as a PNG using only the stdlib.

    The adios2 python env has no matplotlib/PIL, so the renderer is
    self-contained (zlib + struct) rather than pulling in a plotting stack.
    """
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


def _touch_status_image(status_file, image_path):
    """Record the newest rendered frame in the status JSON (for the MCP server)."""
    try:
        with open(status_file) as f:
            s = json.load(f)
        s["image"] = os.path.abspath(image_path)
        tmp = status_file + ".tmp"
        with open(tmp, "w") as f:
            json.dump(s, f)
        os.replace(tmp, status_file)
    except (OSError, json.JSONDecodeError):
        pass


def _get_scalar_int(engine, io, name):
    v = io.InquireVariable(name)
    if not v:
        return None
    try:
        buf = np.zeros(1, dtype=np.int32)
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
