#!/usr/bin/env python3
"""In-situ LBM-CFD MCP server (Vigil reason step).

The reason phase of the trigger-render-reason pipeline for the 2D LBM case.
The hermes variance(vorticity) trigger streams the flagged chaos-onset window
over SST; `lbm_sst_reader.py --status-file <f>` consumes it and writes each
frame's stats to <f>. This MCP server lets an AI agent:

  - inspect_latest_frame()     read the newest streamed frame (vort range,
                               non-finite cells, trigger scalars) to judge chaos
  - fire_rescue_simulation()   RESCUE verdict: revert to the last checkpoint and
                               double the timesteps (lower lattice speed ->
                               restabilize the D2Q9 scheme). Unique to LBM.
  - fire_stop_simulation()     STOP verdict: halt the run early.

Both verdicts write a flag file the simulation polls each output step (run it
with --agent-rescue). No ParaView dependency.

Usage:
  python3 lbm_insitu_mcp_server.py \
      --status-file  <run_dir>/lbm_streaming_status.json \
      --rescue-flag  <run_dir>/lbmcfd.bp.rescue \
      --stop-flag    <run_dir>/lbmcfd.bp.stop

The flag base must match the simulation's ADIOS output name (<out>.rescue /
<out>.stop); the sim polls <out_file>.{rescue,stop} in its run directory.
"""
import argparse
import json
import os
import sys
import logging

logging.basicConfig(level=logging.INFO,
                    format="%(asctime)s - lbm_mcp - %(levelname)s - %(message)s")
logger = logging.getLogger("lbm_mcp")

# Configured in main(); the tool functions read these module globals.
STATUS_FILE = None
RESCUE_FLAG = None
STOP_FLAG = None

SYSTEM_PROMPT = """
You are the reason stage of an in-situ CFD pipeline. A 2D lattice-Boltzmann
simulation streams its vorticity field over ADIOS2 SST only when an in-situ
variance trigger detects the flow destabilising. Your job: inspect the streamed
frames and decide the verdict.

Workflow:
1. get_frame_image() and LOOK at the flow, plus inspect_latest_frame() for the
   numbers. Judge from BOTH.
   - Healthy: smooth, coherent vortices shedding from the barrier into a
     von Karman street.
   - Diverging: dense red/blue salt-and-pepper speckle at grid scale, spreading
     downstream from the barrier; |vorticity| exploding orders of magnitude per
     output; non-finite cells; stable flag 0. That is numerical instability
     (the scheme blowing up), NOT physical turbulence.
2. If it is genuinely diverging and worth saving, call fire_rescue_simulation():
   the sim reverts to its last good checkpoint and halves the lattice speed
   (doubles the timesteps). Re-inspect; if still unstable, rescue again.
3. If the run has nothing left to produce, call fire_stop_simulation() to halt.
Be decisive and brief; every frame of chaos wastes compute. State what you SEE
in the image before issuing a verdict.
"""


def _write_flag(path, kind, reason):
    if not path:
        return (f"No {kind}-flag path configured (start the server with "
                f"--{kind}-flag <out>.{kind}). Cannot issue the {kind} verdict.")
    try:
        with open(path, "w", encoding="utf-8") as f:
            f.write(f"{kind}_by_agent reason={reason}\n")
        logger.info("%s verdict: wrote %s (reason=%s)", kind.upper(), path, reason)
        return (f"{kind.upper()} issued. Wrote {path}. The simulation will act on "
                f"it at its next output step.")
    except OSError as e:
        return f"Failed to write {kind} flag {path}: {e}"


def read_status():
    """Latest streamed frame stats (dict), or None if not available yet."""
    if not STATUS_FILE or not os.path.exists(STATUS_FILE):
        return None
    try:
        with open(STATUS_FILE) as f:
            return json.load(f)
    except (OSError, json.JSONDecodeError):
        return None


def latest_image_path():
    """Path to the newest rendered frame (reader --png-dir), or None."""
    s = read_status()
    if not s:
        return None
    img = s.get("image")
    return img if img and os.path.exists(img) else None


def inspect_latest_frame_impl():
    s = read_status()
    if s is None:
        return ("No frame received yet. The trigger-gated stream ships nothing "
                "until the variance trigger fires; keep polling.")
    absmax = s.get("vort_absmax")
    verdict_hint = "DIVERGING" if (s.get("nonfinite_cells") or
                                   (absmax is not None and absmax > 1e3) or
                                   s.get("stable") == 0) else "nominal"
    return json.dumps({
        "frame": s.get("frame"), "step": s.get("step"), "time": s.get("time"),
        "stable_flag": s.get("stable"),
        "vorticity_min": s.get("vort_min"), "vorticity_max": s.get("vort_max"),
        "vorticity_absmax": absmax, "nonfinite_cells": s.get("nonfinite_cells"),
        "trigger_stat": s.get("trigger_stat"),
        "trigger_fire_step": s.get("trigger_fire_step"),
        "assessment": verdict_hint,
    })


def _build_mcp():
    from mcp.server.fastmcp import FastMCP, Image
    mcp = FastMCP("lbm-insitu")

    @mcp.tool()
    def inspect_latest_frame() -> str:
        """Read the newest streamed LBM frame to judge whether it is going to
        chaos (vorticity range, non-finite cells, trigger stat, stable flag)."""
        return inspect_latest_frame_impl()

    @mcp.tool()
    def get_frame_image():
        """Render of the newest streamed vorticity frame -- LOOK at it.

        A healthy run shows smooth, coherent vortices shedding from the barrier
        (a von Karman street). A diverging run shows dense red/blue
        salt-and-pepper speckle at grid scale, spreading downstream from the
        barrier -- that is numerical instability, not turbulence.
        """
        img = latest_image_path()
        if not img:
            return ("No frame image available yet (the gated stream ships "
                    "nothing until the trigger fires; keep polling).")
        return Image(path=img)

    @mcp.tool()
    def fire_rescue_simulation(reason: str = "") -> str:
        """RESCUE verdict: revert the simulation to its last checkpoint and
        double the timesteps (halve the lattice speed) to restabilize the
        scheme. Use once you have confirmed numerical instability. Re-inspect
        after; rescue again if still diverging."""
        return _write_flag(RESCUE_FLAG, "rescue", reason)

    @mcp.tool()
    def fire_stop_simulation(reason: str = "") -> str:
        """STOP verdict: halt the run early (nothing left worth computing)."""
        return _write_flag(STOP_FLAG, "stop", reason)

    return mcp


def main():
    global STATUS_FILE, RESCUE_FLAG, STOP_FLAG
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--status-file", required=True,
                    help="JSON status file written by lbm_sst_reader.py --status-file")
    ap.add_argument("--rescue-flag", default=None,
                    help="path to the sim's <out>.rescue flag")
    ap.add_argument("--stop-flag", default=None,
                    help="path to the sim's <out>.stop flag")
    ap.add_argument("--selftest", action="store_true",
                    help="write both flags + read status, then exit (no MCP)")
    args = ap.parse_args()
    STATUS_FILE = args.status_file
    RESCUE_FLAG = args.rescue_flag
    STOP_FLAG = args.stop_flag

    if args.selftest:
        print("inspect:", inspect_latest_frame_impl())
        print("image  :", latest_image_path() or "(none)")
        print("rescue :", _write_flag(RESCUE_FLAG, "rescue", "selftest"))
        print("stop   :", _write_flag(STOP_FLAG, "stop", "selftest"))
        return 0

    logger.info("LBM in-situ MCP server: status=%s rescue=%s stop=%s",
                STATUS_FILE, RESCUE_FLAG, STOP_FLAG)
    _build_mcp().run(transport="stdio")
    return 0


if __name__ == "__main__":
    sys.exit(main())
