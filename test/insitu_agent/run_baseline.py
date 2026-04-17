"""
Baseline runner: visualizes every K SST output steps with a fixed pipeline
(isosurface(V, 0.3) + screenshot). No LLM. Drives the in-situ MCP server
via stdio, identical transport to insitu_agent.py — only difference is the
"decision" is a fixed schedule instead of an LLM.

Usage:
  python3 run_baseline.py \
      --K 2 \
      --results-dir results/bl_vs_ag/BL_K2 \
      --pvpython /path/to/pvpython \
      --server-host ares-comp-21 \
      --server-port 11112
"""

import argparse
import asyncio
import base64
import json
import os
import sys
import time
from pathlib import Path

from mcp import ClientSession, StdioServerParameters
from mcp.client.stdio import stdio_client


async def _call(session, name, args=None):
    """Call an MCP tool and return (result_text, image_bytes_or_None, elapsed_ms)."""
    t0 = time.monotonic()
    result = await session.call_tool(name, arguments=args or {})
    elapsed_ms = (time.monotonic() - t0) * 1000

    text = ""
    image_bytes = None
    for block in result.content:
        if hasattr(block, "text") and block.text:
            text += block.text
        if hasattr(block, "data") and block.data:
            try:
                image_bytes = base64.b64decode(block.data)
            except Exception:
                image_bytes = None
    return text, image_bytes, elapsed_ms


def _parse_status(text):
    """Parse 'Streaming status:\\nCurrent timestep: N\\nPaused: ...\\nStream ended: ...'."""
    info = {}
    for line in text.splitlines():
        line = line.strip()
        if line.startswith("Current timestep:"):
            v = line.split(":", 1)[1].strip()
            try:
                info["step"] = int(v)
            except ValueError:
                info["step"] = -1
        elif line.startswith("Paused:"):
            info["paused"] = "true" in line.lower() or "True" in line
        elif line.startswith("Stream ended:"):
            info["ended"] = "true" in line.lower() or "True" in line
        elif line.startswith("Pipeline ready:"):
            info["pipeline_ready"] = "true" in line.lower() or "True" in line
    return info


async def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--K", type=int, required=True, help="Visualize every K SST output steps")
    parser.add_argument("--results-dir", type=str, required=True)
    parser.add_argument("--pvpython", type=str, required=True)
    parser.add_argument("--server-host", type=str, default="localhost")
    parser.add_argument("--server-port", type=int, default=11112)
    parser.add_argument("--max-wait-per-step", type=float, default=120.0,
                        help="Max seconds to wait for the bridge to advance one step")
    parser.add_argument("--isovalue", type=float, default=0.3)
    parser.add_argument("--field", type=str, default="V")
    parser.add_argument("--screenshot-file", type=str, default=None,
                        help="Bridge-saved PNG path (passed to MCP server)")
    args = parser.parse_args()

    results_dir = Path(args.results_dir)
    screenshots_dir = results_dir / "screenshots"
    screenshots_dir.mkdir(parents=True, exist_ok=True)

    metrics_path = results_dir / "baseline_metrics.jsonl"
    if metrics_path.exists():
        metrics_path.unlink()

    script_dir = Path(__file__).resolve().parent
    mcp_server = str(script_dir / "insitu_mcp_server.py")
    status_file = str(script_dir / "streaming_status.json")
    timing_file = str(results_dir / "mcp_tool_timing.jsonl")

    mcp_cli_args = [
        mcp_server,
        "--server", args.server_host,
        "--port", str(args.server_port),
        "--status-file", status_file,
        "--timing-file", timing_file,
    ]
    if args.screenshot_file:
        mcp_cli_args.extend(["--screenshot-file", args.screenshot_file])

    server_params = StdioServerParameters(
        command=args.pvpython,
        args=mcp_cli_args,
        env={**os.environ},
    )

    print(f"[baseline] K={args.K}  results={results_dir}")
    print(f"[baseline] launching MCP: {args.pvpython} {mcp_server}")

    wall_start = time.monotonic()
    iso_created = False
    last_step = -1
    total_advances = 0
    total_screenshots = 0
    total_image_bytes = 0

    async with stdio_client(server_params) as (read, write):
        async with ClientSession(read, write) as session:
            await session.initialize()
            tools_resp = await session.list_tools()
            print(f"[baseline] {len(tools_resp.tools)} MCP tools available")

            # initial status
            txt, _, _ = await _call(session, "get_streaming_status")
            st = _parse_status(txt)
            print(f"[baseline] initial status: {st}")
            last_step = st.get("step", 0)

            while True:
                # Check whether the stream has ended
                txt, _, _ = await _call(session, "get_streaming_status")
                st = _parse_status(txt)
                if st.get("ended"):
                    print(f"[baseline] stream ended at step {st.get('step')}")
                    break

                # Advance one step
                t_advance0 = time.monotonic()
                _txt, _, advance_ms = await _call(session, "advance_step")
                total_advances += 1

                # Wait for the bridge to actually process the advance command
                target_step = last_step + 1
                wait_start = time.monotonic()
                while True:
                    txt, _, _ = await _call(session, "get_streaming_status")
                    st = _parse_status(txt)
                    cur_step = st.get("step", -1)
                    if cur_step >= target_step:
                        break
                    if st.get("ended"):
                        break
                    if time.monotonic() - wait_start > args.max_wait_per_step:
                        print(f"[baseline] timeout waiting for advance to {target_step}")
                        break
                    await asyncio.sleep(0.2)

                advance_total_ms = (time.monotonic() - t_advance0) * 1000

                if st.get("ended"):
                    print(f"[baseline] stream ended while waiting for step {target_step}")
                    break

                last_step = st.get("step", target_step)
                output_idx = last_step  # 1-indexed SST output step

                # Decide whether to visualize
                visualize = (output_idx % args.K == 0)

                record = {
                    "output_step": output_idx,
                    "visualize": visualize,
                    "advance_total_ms": round(advance_total_ms, 2),
                    "timestamp": time.time(),
                }

                if visualize:
                    # First-time setup of the contour pipeline
                    if not iso_created:
                        iso_txt, _, iso_ms = await _call(
                            session, "create_isosurface",
                            {"value": args.isovalue, "field": args.field},
                        )
                        record["create_isosurface_ms"] = round(iso_ms, 2)
                        record["create_isosurface_msg"] = iso_txt[:200]
                        iso_created = True
                        print(f"[baseline] step {output_idx}: created isosurface ({iso_ms:.1f} ms)")

                    # Take a screenshot
                    ss_txt, img_bytes, ss_ms = await _call(session, "get_screenshot")
                    record["get_screenshot_ms"] = round(ss_ms, 2)

                    if img_bytes:
                        out_png = screenshots_dir / f"step_{output_idx:04d}.png"
                        out_png.write_bytes(img_bytes)
                        record["screenshot_path"] = str(out_png.relative_to(results_dir))
                        record["image_bytes"] = len(img_bytes)
                        total_image_bytes += len(img_bytes)
                        total_screenshots += 1
                        print(f"[baseline] step {output_idx}: screenshot {len(img_bytes)} bytes ({ss_ms:.1f} ms)")
                    else:
                        record["screenshot_path"] = None
                        record["image_bytes"] = 0
                        print(f"[baseline] step {output_idx}: NO IMAGE BYTES — text='{ss_txt[:120]}'")

                with open(metrics_path, "a") as f:
                    json.dump(record, f)
                    f.write("\n")

    wall_total = time.monotonic() - wall_start
    summary = {
        "K": args.K,
        "wall_time_s": round(wall_total, 3),
        "num_advance_calls": total_advances,
        "num_screenshots": total_screenshots,
        "total_image_bytes": total_image_bytes,
        "last_step": last_step,
    }
    with open(results_dir / "baseline_summary.json", "w") as f:
        json.dump(summary, f, indent=2)

    print(f"[baseline] done. {summary}")


if __name__ == "__main__":
    asyncio.run(main())
