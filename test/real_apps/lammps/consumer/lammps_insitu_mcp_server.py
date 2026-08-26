#!/usr/bin/env python3
"""In-situ LAMMPS MCP server (Vigil reason step).

The reason phase of the trigger-render-reason pipeline for the LAMMPS
velocity-Verlet-failure case. The hermes `mean` trigger on the kinetic
temperature (`derive/V2mean` = 3*T*) streams the super-heated window over SST;
`lammps_sst_reader.py --status-file <f>` consumes it, renders each flagged step
as an atom scatter (colored by speed), and writes the frame stats to <f>. This
MCP server lets an AI agent:

  - inspect_latest_frame()   read the newest streamed frame (kinetic
                             temperature T*, max speed, non-finite/escaped
                             atoms, trigger scalars) to judge the blow-up
  - get_frame_image()        LOOK at the rendered atom scatter
  - fire_stop_simulation()   STOP verdict: the integration has diverged
                             (temperature runaway -> "Lost atoms"); halt.

fire_stop writes a `<out>.stop` flag file. NOTE: stock `lmp` does not itself
poll this flag -- wiring the LAMMPS writer (or the engine) to halt on it is the
remaining integration step; the verdict + flag are produced here so the reason
stage is complete and testable. No ParaView dependency.

Usage:
  python3 lammps_insitu_mcp_server.py \
      --status-file <run_dir>/lammps_status.json \
      --stop-flag   <run_dir>/lammps.bp.stop
"""
import argparse
import json
import os
import sys
import logging

logging.basicConfig(level=logging.INFO,
                    format="%(asctime)s - lammps_mcp - %(levelname)s - %(message)s")
logger = logging.getLogger("lammps_mcp")

# Configured in main(); the tool functions read these module globals.
STATUS_FILE = None
STOP_FLAG = None

SYSTEM_PROMPT = """
You are the reason stage of an in-situ molecular-dynamics pipeline. A LAMMPS
Lennard-Jones simulation streams its atom positions + velocities over ADIOS2 SST
only when an in-situ mean(|v|^2) (kinetic-temperature) trigger detects the
velocity-Verlet integration going unstable: the timestep is too large, so the
temperature runs away from its set point T* ~ 0.75 and the atoms fly apart until
LAMMPS aborts with "Lost atoms". Your job: inspect the streamed frames and decide
the verdict.

Workflow:
1. get_frame_image() and LOOK at the atoms, plus inspect_latest_frame() for the
   numbers. Judge from BOTH.
   - Healthy: atoms on/near the FCC lattice, roughly uniform low speed (blue),
     T* ~ 0.75.
   - Diverging: atoms scattering / flying apart, a few atoms with huge speed
     (bright yellow/red), T* exploding orders of magnitude above 0.75, non-finite
     or escaped atoms. That is a numerical integration blow-up, NOT physical
     heating.
2. If it is genuinely diverging and unrecoverable, call fire_stop_simulation().
Be decisive and brief; every super-heated step wastes compute. State what you
SEE in the image before issuing the verdict.
"""


def _write_flag(path, kind, reason):
    if not path:
        return (f"No {kind}-flag path configured (start the server with "
                f"--{kind}-flag <out>.{kind}). Cannot issue the {kind} verdict.")
    try:
        with open(path, "w", encoding="utf-8") as f:
            f.write(f"{kind}_by_agent reason={reason}\n")
        logger.info("%s verdict: wrote %s (reason=%s)", kind.upper(), path, reason)
        return (f"{kind.upper()} issued. Wrote {path}. (The simulation halts on "
                f"it once the writer is wired to poll the flag.)")
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
                "until the kinetic-temperature trigger fires; keep polling.")
    tstar = s.get("kinetic_temperature")
    ratio = s.get("tstar_ratio")
    diverging = (s.get("nonfinite_atoms") or
                 (ratio is not None and ratio > 2.0) or
                 (tstar is not None and tstar > 1.5))
    return json.dumps({
        "frame": s.get("frame"), "step": s.get("step"),
        "n_atoms": s.get("n_atoms"),
        "kinetic_temperature_Tstar": tstar,
        "Tstar_setpoint": s.get("tstar_setpoint"),
        "Tstar_ratio_vs_setpoint": ratio,
        "speed_max": s.get("speed_max"), "speed_mean": s.get("speed_mean"),
        "nonfinite_atoms": s.get("nonfinite_atoms"),
        "derive_V2mean_3Tstar": s.get("v2mean_derived"),
        "trigger_stat": s.get("trigger_stat"),
        "trigger_fire_step": s.get("trigger_fire_step"),
        "assessment": "DIVERGING" if diverging else "nominal",
    })


def _build_mcp():
    from mcp.server.fastmcp import FastMCP, Image
    mcp = FastMCP("lammps-insitu")

    @mcp.tool()
    def inspect_latest_frame() -> str:
        """Read the newest streamed LAMMPS frame to judge whether the integration
        is blowing up (kinetic temperature T*, max speed, non-finite/escaped
        atoms, trigger stat)."""
        return inspect_latest_frame_impl()

    @mcp.tool()
    def get_frame_image():
        """Render of the newest streamed atom scatter -- LOOK at it.

        A healthy run shows atoms on/near the FCC lattice, uniform low speed
        (blue). A diverging run shows atoms flying apart with a few very fast
        atoms (yellow/red) -- the velocity-Verlet integration blowing up, not
        physical heating.
        """
        img = latest_image_path()
        if not img:
            return ("No frame image available yet (the gated stream ships "
                    "nothing until the trigger fires; keep polling).")
        return Image(path=img)

    @mcp.tool()
    def fire_stop_simulation(reason: str = "") -> str:
        """STOP verdict: the integration has diverged (temperature runaway ->
        'Lost atoms'); halt the run early. Use once you have visually confirmed
        the atoms are flying apart / T* has exploded."""
        return _write_flag(STOP_FLAG, "stop", reason)

    return mcp


def main():
    global STATUS_FILE, STOP_FLAG
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--status-file", required=True,
                    help="JSON status file written by lammps_sst_reader.py --status-file")
    ap.add_argument("--stop-flag", default=None,
                    help="path to the sim's <out>.stop flag")
    ap.add_argument("--selftest", action="store_true",
                    help="print status + image path + write the stop flag, then exit (no MCP)")
    args = ap.parse_args()
    STATUS_FILE = args.status_file
    STOP_FLAG = args.stop_flag

    if args.selftest:
        print("inspect:", inspect_latest_frame_impl())
        print("image  :", latest_image_path() or "(none)")
        print("stop   :", _write_flag(STOP_FLAG, "stop", "selftest"))
        return 0

    logger.info("LAMMPS in-situ MCP server: status=%s stop=%s", STATUS_FILE, STOP_FLAG)
    _build_mcp().run(transport="stdio")
    return 0


if __name__ == "__main__":
    sys.exit(main())
