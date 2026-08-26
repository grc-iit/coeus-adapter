#!/usr/bin/env python3
"""AI agent for the LBM-CFD reason step (Vigil trigger-render-reason).

Connects to lbm_insitu_mcp_server.py over MCP (stdio), lets Claude LOOK at the
streamed vorticity frames plus the numeric stats, and issues the verdict
(fire_rescue_simulation / fire_stop_simulation) when it recognizes the flow is
diverging. The verdict writes a flag the simulation polls (run it with
--agent-rescue).

The API key is read from the environment (ANTHROPIC_API_KEY, optionally with
ANTHROPIC_BASE_URL for a proxy) by the SDK -- never passed on the command line
and never written to disk.

Usage:
  export ANTHROPIC_API_KEY=sk-ant-...          # never commit this
  python3 lbm_agent.py \
      --status-file <run_dir>/lbm_streaming_status.json \
      --rescue-flag <run_dir>/lbmcfd.bp.rescue \
      --stop-flag   <run_dir>/lbmcfd.bp.stop
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
You are the reason stage of an in-situ CFD pipeline. A 2D lattice-Boltzmann
simulation streams its vorticity field over ADIOS2 SST only when an in-situ
variance trigger detects the flow destabilising. Decide the verdict.

Judge from BOTH the image and the numbers:
- Healthy: smooth, coherent vortices shedding from the barrier (a von Karman
  street).
- Diverging: dense red/blue salt-and-pepper speckle at grid scale, spreading
  downstream from the barrier; |vorticity| exploding orders of magnitude per
  output; non-finite cells; stable flag 0. That is numerical instability (the
  scheme blowing up), NOT physical turbulence.

Workflow:
1. Call get_frame_image and inspect_latest_frame. State plainly what you SEE in
   the image before deciding.
2. If the run is genuinely diverging and worth saving, call
   fire_rescue_simulation: the sim reverts to its last good checkpoint and
   halves the lattice speed (doubles the timesteps) to restabilise.
3. If the run has nothing left worth computing, call fire_stop_simulation.
Be decisive and brief -- every frame of chaos wastes compute. Issue exactly one
verdict, then stop and summarize why in one or two sentences.
""".strip()


async def run(args):
    import anthropic

    mcp_args = [str(SCRIPT_DIR / "lbm_insitu_mcp_server.py"),
                "--status-file", args.status_file]
    if args.rescue_flag:
        mcp_args += ["--rescue-flag", args.rescue_flag]
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
            if c.name in ("fire_rescue_simulation", "fire_stop_simulation"):
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
    ap.add_argument("--rescue-flag", default=None)
    ap.add_argument("--stop-flag", default=None)
    ap.add_argument("--model", default="claude-opus-4-8")
    ap.add_argument("--max-turns", type=int, default=8)
    ap.add_argument("--prompt", default="Inspect the streamed simulation frame "
                    "and decide whether it is going to chaos. Issue your verdict.")
    args = ap.parse_args()
    if not os.environ.get("ANTHROPIC_API_KEY") and not os.environ.get("ANTHROPIC_AUTH_TOKEN"):
        sys.exit("ANTHROPIC_API_KEY is not set (export it; never commit it).")
    return asyncio.run(run(args))


if __name__ == "__main__":
    sys.exit(main())
