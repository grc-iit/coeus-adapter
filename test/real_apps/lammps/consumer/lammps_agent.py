#!/usr/bin/env python3
"""AI agent for the LAMMPS reason step (Vigil trigger-render-reason).

Connects to lammps_insitu_mcp_server.py over MCP (stdio), lets Claude LOOK at
the streamed atom-scatter frames plus the numeric stats (kinetic temperature,
max speed, escaped/non-finite atoms), and issues the verdict
(fire_stop_simulation) once it recognizes the velocity-Verlet integration is
blowing up. The verdict writes a `<out>.stop` flag.

The API key is read from the environment (ANTHROPIC_API_KEY, optionally with
ANTHROPIC_BASE_URL for a proxy) by the SDK -- never passed on the command line
and never written to disk.

Usage:
  export ANTHROPIC_API_KEY=sk-ant-...          # never commit this
  python3 lammps_agent.py \
      --status-file <run_dir>/lammps_status.json \
      --stop-flag   <run_dir>/lammps.bp.stop
"""
import argparse
import asyncio
import base64
import json
import os
import sys
from pathlib import Path

from mcp import ClientSession, StdioServerParameters
from mcp.client.stdio import stdio_client

SCRIPT_DIR = Path(__file__).resolve().parent

SYSTEM_PROMPT = """
You are the reason stage of an in-situ molecular-dynamics pipeline. A LAMMPS
Lennard-Jones simulation streams its atom positions + velocities over ADIOS2 SST
only when an in-situ mean(|v|^2) (kinetic-temperature) trigger detects the
velocity-Verlet integration going unstable (timestep too large -> temperature
runs away from T* ~ 0.75 and the atoms fly apart until LAMMPS aborts with
"Lost atoms"). Decide the verdict.

Judge from BOTH the image and the numbers:
- Healthy: atoms on/near the FCC lattice, roughly uniform low speed (blue),
  T* ~ 0.75.
- Diverging: atoms scattering / flying apart, a few atoms with huge speed
  (bright yellow/red), T* exploding orders of magnitude above 0.75, non-finite
  or escaped atoms. That is a numerical integration blow-up, NOT physical heating.

Workflow:
1. Call get_frame_image and inspect_latest_frame. State plainly what you SEE in
   the image before deciding.
2. If the run is genuinely diverging, call fire_stop_simulation.
Be decisive and brief -- every super-heated step wastes compute. Issue exactly
one verdict, then stop and summarize why in one or two sentences.
""".strip()


async def run(args):
    import anthropic

    mcp_args = [str(SCRIPT_DIR / "lammps_insitu_mcp_server.py"),
                "--status-file", args.status_file]
    if args.stop_flag:
        mcp_args += ["--stop-flag", args.stop_flag]

    server = StdioServerParameters(command=sys.executable, args=mcp_args, env=os.environ.copy())
    client = anthropic.AsyncAnthropic()

    # Adaptive thinking exists only on Opus 4.6+/Sonnet 5/Sonnet 4.6/Fable/Mythos;
    # Haiku 4.5 (and older) reject it, so omit the parameter there.
    adaptive = any(k in args.model for k in
                   ("opus-4-6", "opus-4-7", "opus-4-8", "sonnet-5",
                    "sonnet-4-6", "fable", "mythos"))

    async def ask(msgs, tools):
        kw = dict(model=args.model, max_tokens=4096, system=SYSTEM_PROMPT,
                  messages=msgs, tools=tools)
        if adaptive:
            kw["thinking"] = {"type": "adaptive"}
        return await client.messages.create(**kw)

    verdict = None
    try:
        async with stdio_client(server) as (read, write):
            async with ClientSession(read, write) as session:
                verdict = await _drive(session, args, ask)
    except BaseException:
        # The mcp stdio transport can raise a benign teardown error on close;
        # only re-raise if we never reached a verdict.
        if verdict is None:
            raise

    print("[agent] VERDICT: %s" % (verdict or "none issued"), flush=True)
    return 0 if verdict else 2


async def _drive(session, args, ask):
    await session.initialize()
    listed = await session.list_tools()
    tools = [{"name": t.name,
              "description": t.description or "",
              "input_schema": t.inputSchema or {"type": "object", "properties": {}}}
             for t in listed.tools]
    print("[agent] tools: %s" % ", ".join(t["name"] for t in tools), flush=True)

    messages = [{"role": "user", "content": args.prompt}]
    verdict = None

    for turn in range(args.max_turns):
        resp = await ask(messages, tools)
        # Echo content back unchanged (thinking blocks must round-trip).
        messages.append({"role": "assistant", "content": resp.content})

        for b in resp.content:
            if b.type == "text" and b.text.strip():
                print("\n[agent] %s\n" % b.text.strip(), flush=True)

        calls = [b for b in resp.content if b.type == "tool_use"]
        if not calls:
            break

        results = []
        for c in calls:
            print("[agent]  -> %s(%s)" % (c.name, json.dumps(c.input)), flush=True)
            if c.name == "fire_stop_simulation":
                verdict = c.name
            out = await session.call_tool(c.name, arguments=c.input or {})

            blocks = []
            for blk in out.content:
                if getattr(blk, "text", None):
                    print("[agent]  <- %s" % blk.text[:160], flush=True)
                    blocks.append({"type": "text", "text": blk.text})
                elif getattr(blk, "data", None):
                    # Hand the actual image to the model so it can SEE it.
                    n = len(base64.b64decode(blk.data))
                    print("[agent]  <- [image %d bytes]" % n, flush=True)
                    blocks.append({"type": "image", "source": {
                        "type": "base64", "media_type": "image/png", "data": blk.data}})
            if not blocks:
                blocks = [{"type": "text", "text": "(no content)"}]
            results.append({"type": "tool_result", "tool_use_id": c.id, "content": blocks})

        messages.append({"role": "user", "content": results})

        if verdict:  # let it summarize after the verdict, then stop
            final = await ask(messages, tools)
            for b in final.content:
                if b.type == "text" and b.text.strip():
                    print("\n[agent] %s\n" % b.text.strip(), flush=True)
            break

    return verdict


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--status-file", required=True)
    ap.add_argument("--stop-flag", default=None)
    ap.add_argument("--model", default="claude-opus-4-8")
    ap.add_argument("--max-turns", type=int, default=8)
    ap.add_argument("--prompt", default="Inspect the streamed LAMMPS atom frame "
                    "and decide whether the integration is blowing up. Issue your verdict.")
    args = ap.parse_args()
    if not os.environ.get("ANTHROPIC_API_KEY") and not os.environ.get("ANTHROPIC_AUTH_TOKEN"):
        sys.exit("ANTHROPIC_API_KEY is not set (export it; never commit it).")
    return asyncio.run(run(args))


if __name__ == "__main__":
    sys.exit(main())
