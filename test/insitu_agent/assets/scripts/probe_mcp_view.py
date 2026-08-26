"""
No-LLM probe for the MCP-view stale-render bug and its fix (_refresh_mcp_view).

Measured 2026-07-15: over a 4-step window the bridge rendered 4/4 distinct
frames while the MCP client's own view (the one get_screenshot serves once the
agent creates an isosurface) rendered only 2/4 — the agent reasons on frozen
images and can flip its verdict (the 64-rank NO-FIRE mistake).

This probe drives the REAL insitu_mcp_server tool functions in-process (the
FastMCP decorators return the plain functions), so the code path exercised is
exactly what an agent hits:

  1. connect as a second pvserver client, like the MCP server does
  2. advance one step so the bridge builds its pipeline
  3. create_isosurface on the shared live source
  4. advance N more steps via the bridge's command/status protocol
  5. after each confirmed step, save BOTH the bridge PNG and the MCP-view PNG
  6. report distinct-frame counts + per-step V range (scalar ground truth)

PASS: bridge N/N distinct AND MCP view N/N distinct.
With INSITU_MCP_NO_REFRESH=1 the fix is disabled and the stale bug should
reproduce (MCP view < N distinct while V's range keeps changing).

Run with the spack python + paraview PYTHONPATH/LD_LIBRARY_PATH (same env as
the MCP server itself; see reference_insitu_agent_pipeline memory / README).
"""

import argparse
import hashlib
import json
import os
import shutil
import sys

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')))
import insitu_mcp_server as ims


def md5_of(path):
    h = hashlib.md5()
    with open(path, "rb") as f:
        h.update(f.read())
    return h.hexdigest()


def find_fides_source():
    from paraview.simple import GetSources
    for _key, src in GetSources().items():
        try:
            if "fides" in (src.SMProxy.GetXMLName() or "").lower():
                return src
        except Exception:
            continue
    return None


def v_range():
    """Server-side V range as seen from THIS client — the scalar ground truth
    for 'did the data change this step' that pixels cannot provide."""
    src = find_fides_source()
    if src is None:
        return None
    try:
        info = src.GetPointDataInformation().GetArray("V")
        if info is None:
            return None
        return tuple(info.GetComponentRange(0))
    except Exception:
        return None


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[1])
    ap.add_argument("--steps", type=int, default=4,
                    help="steps to advance AFTER the isosurface exists")
    ap.add_argument("--iso", type=float, default=0.3)
    ap.add_argument("--out", required=True, help="dir for per-step PNGs + report")
    ap.add_argument("--server", default="localhost")
    ap.add_argument("--port", type=int, default=11112)
    ap.add_argument("--status-file", required=True,
                    help="bridge status JSON (absolute path)")
    ap.add_argument("--screenshot-file", required=True,
                    help="bridge PNG path (absolute)")
    ap.add_argument("--timeout", type=float, default=300.0)
    args = ap.parse_args()

    os.makedirs(args.out, exist_ok=True)
    ims.STATUS_FILE_PATH = os.path.abspath(args.status_file)
    ims.SCREENSHOT_FILE = os.path.abspath(args.screenshot_file)
    ims._pv_server = args.server
    ims._pv_port = args.port

    no_refresh = os.environ.get("INSITU_MCP_NO_REFRESH") == "1"
    print(f"[probe] refresh fix {'DISABLED (bug repro mode)' if no_refresh else 'ENABLED'}")

    # Land the first step so the bridge builds its pipeline (it starts --paused).
    print("[probe] " + ims.advance_step(timeout_s=args.timeout))

    if not ims._ensure_connected():
        sys.exit("[probe] FATAL: could not connect to pvserver")

    # create_isosurface contours the active source; make sure that resolves to
    # the shared Fides reader in this fresh client session.
    from paraview.simple import GetActiveSource, SetActiveSource
    active = GetActiveSource()
    is_fides = False
    try:
        is_fides = active is not None and \
            "fides" in (active.SMProxy.GetXMLName() or "").lower()
    except Exception:
        pass
    if not is_fides:
        fides = find_fides_source()
        if fides is None:
            sys.exit("[probe] FATAL: no Fides source visible in this client "
                     "(is the bridge past step 1?)")
        SetActiveSource(fides)
    fides = GetActiveSource()
    print(f"[probe] contour input: {fides.SMProxy.GetXMLName()} "
          f"id={fides.SMProxy.GetGlobalIDAsString()}")

    print("[probe] " + str(ims.create_isosurface(args.iso, "V")))

    rows = []
    for _ in range(args.steps):
        msg = ims.advance_step(timeout_s=args.timeout)
        print("[probe] " + msg)
        if "Advanced to timestep" not in msg:
            sys.exit(f"[probe] FATAL: advance did not land a step: {msg}")
        with open(ims.STATUS_FILE_PATH) as f:
            step = json.load(f)["step"]

        bridge_png = os.path.join(args.out, f"bridge_step{step:03d}.png")
        shutil.copyfile(ims.SCREENSHOT_FILE, bridge_png)

        shot = ims.get_screenshot()
        shot_path = getattr(shot, "path", None)
        if shot_path is None:
            sys.exit(f"[probe] FATAL: get_screenshot returned no image: {shot}")
        mcp_png = os.path.join(args.out, f"mcp_step{step:03d}.png")
        shutil.copyfile(str(shot_path), mcp_png)

        vr = v_range()
        row = {
            "step": step,
            "v_range": vr,
            "bridge_md5": md5_of(bridge_png),
            "bridge_bytes": os.path.getsize(bridge_png),
            "mcp_md5": md5_of(mcp_png),
            "mcp_bytes": os.path.getsize(mcp_png),
        }
        rows.append(row)
        vr_txt = f"[{vr[0]:.6f},{vr[1]:.6f}]" if vr else "n/a"
        print(f"[probe] step {step}: V={vr_txt} "
              f"bridge={row['bridge_md5'][:8]}/{row['bridge_bytes']}B "
              f"mcp={row['mcp_md5'][:8]}/{row['mcp_bytes']}B")

    n = len(rows)
    bridge_distinct = len({r["bridge_md5"] for r in rows})
    mcp_distinct = len({r["mcp_md5"] for r in rows})
    v_distinct = len({r["v_range"] for r in rows})
    verdict = "PASS" if (bridge_distinct == n and mcp_distinct == n) else "FAIL"

    report = {
        "refresh_fix_enabled": not no_refresh,
        "steps": n,
        "bridge_distinct": bridge_distinct,
        "mcp_distinct": mcp_distinct,
        "v_range_distinct": v_distinct,
        "verdict": verdict,
        "rows": rows,
    }
    with open(os.path.join(args.out, "report.json"), "w") as f:
        json.dump(report, f, indent=2)

    print(f"[probe] RESULT: bridge {bridge_distinct}/{n} distinct, "
          f"MCP view {mcp_distinct}/{n} distinct, "
          f"V-range {v_distinct}/{n} distinct -> {verdict}")
    sys.exit(0 if verdict == "PASS" else 2)


if __name__ == "__main__":
    main()
