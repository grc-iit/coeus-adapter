#!/usr/bin/env python3
"""
ParaView MCP benchmark runner.

For each case in the manifest:
  1. Spawn paraview_mcp_server.py over stdio (which connects to pvserver).
  2. Drive a Claude model via the Anthropic API in a tool-use loop.
  3. Forward each tool_use block to the MCP server, return the result.
  4. Stop when the model emits a final text answer (or max_turns is hit).
  5. Dump the full transcript to JSON for later scoring.

Required:
  - pvserver --multi-clients running on $PV_HOST:$PV_PORT (default localhost:11111)
  - ANTHROPIC_API_KEY in the environment

Run:
    pvpython benchmark/run_benchmark.py --case 0
    pvpython benchmark/run_benchmark.py --all
"""
import io
import os
import sys

# pvpython replaces sys.stdout/sys.stderr with VTK stream-capture objects
# that lack a real fileno(). The mcp stdio client wires the child process's
# stderr to OUR stderr, so it must be a real fd. Restore real stdout/stderr
# before importing anything that touches them.
_real_stdout = os.fdopen(os.dup(1), "wb", buffering=0)
_real_stderr = os.fdopen(os.dup(2), "wb", buffering=0)
sys.stdout = io.TextIOWrapper(_real_stdout, write_through=True)
sys.stderr = io.TextIOWrapper(_real_stderr, write_through=True)

import argparse
import asyncio
import json
import shutil
from pathlib import Path

import anthropic
from mcp import ClientSession, StdioServerParameters
from mcp.client.stdio import stdio_client


REPO = Path(__file__).resolve().parent.parent
# LLNL ParaView-MCP server location: $PARAVIEW_MCP (dir holding
# paraview_mcp_server.py), default ~/software/paraview_mcp; override with
# --mcp-server.
MCP_DIR = Path(os.environ.get("PARAVIEW_MCP", Path.home() / "software" / "paraview_mcp"))
MCP_SERVER_PATH = MCP_DIR / "paraview_mcp_server.py"
# pvpython: $PVPYTHON, else the pvpython on PATH; override with --pvpython.
DEFAULT_PVPYTHON = shutil.which("pvpython") or "pvpython"

SYSTEM_PROMPT = """You are an autonomous scientific visualization agent with
access to ParaView through MCP tools. The user will give you a dataset and a
question. Use the ParaView tools to investigate the data and answer.

Guidelines:
- ParaView is already connected to the server on startup; do NOT try to connect.
- Call ONE MCP tool per turn, observe the result, then decide the next step.
- Prefer non-visual diagnostic tools (get_available_arrays, get_histogram,
  plot_over_line, get_pipeline) when they are sufficient.
- When you have enough information, give your final answer in plain text and
  STOP calling tools. Be concrete: report (x, y, z) integer voxels for any
  spatial coordinates the question asks for.
- If a tool fails, read the error and adapt; do not retry the identical call.
"""


def mcp_tools_to_anthropic(tools_response):
    """Convert mcp.ListToolsResult to Anthropic tools format."""
    out = []
    for t in tools_response.tools:
        schema = t.inputSchema or {"type": "object", "properties": {}}
        out.append({
            "name": t.name,
            "description": (t.description or "").strip(),
            "input_schema": schema,
        })
    return out


def mcp_result_to_anthropic_blocks(result):
    """
    Convert an MCP CallToolResult to a list of Anthropic content blocks
    suitable for use as the `content` of a `tool_result`. Forwards image
    content as actual image blocks so the vision model can see them.
    """
    blocks = []
    for item in result.content:
        if hasattr(item, "text") and item.text is not None:
            blocks.append({"type": "text", "text": item.text})
        elif getattr(item, "type", None) == "image":
            blocks.append({
                "type": "image",
                "source": {
                    "type": "base64",
                    "media_type": getattr(item, "mimeType", "image/png"),
                    "data": item.data,
                },
            })
        else:
            blocks.append({"type": "text", "text": f"[{type(item).__name__}]"})
    if not blocks:
        blocks = [{"type": "text", "text": ""}]
    return blocks


def stringify_for_log(blocks):
    """One-line-ish summary of tool-result blocks for the transcript JSON."""
    parts = []
    for b in blocks:
        if b["type"] == "text":
            parts.append(b["text"])
        elif b["type"] == "image":
            mt = b["source"].get("media_type", "image/?")
            n = len(b["source"].get("data", ""))
            parts.append(f"[image {mt} ~{n}b base64]")
    return "\n".join(parts)


def serializable_content(content):
    """Convert Anthropic response content blocks to JSON-friendly dicts."""
    out = []
    for b in content:
        if b.type == "text":
            out.append({"type": "text", "text": b.text})
        elif b.type == "tool_use":
            out.append({
                "type": "tool_use",
                "id": b.id,
                "name": b.name,
                "input": b.input,
            })
        else:
            out.append({"type": b.type})
    return out


async def run_case(case, model, max_turns, max_tokens, pv_host, pv_port,
                   pvpython, mcp_server_path, out_dir, verbose=False):
    server_params = StdioServerParameters(
        command=pvpython,
        args=[
            str(mcp_server_path),
            "--server", pv_host,
            "--port", str(pv_port),
        ],
        env=os.environ.copy(),
    )

    transcript = {
        "case_id": case["case_id"],
        "needle": case["needle"],
        "task": case["task"],
        "model": model,
        "max_turns": max_turns,
        "turns": [],
        "tool_calls": [],
        "usage": {
            "input_tokens": 0,
            "output_tokens": 0,
            "cache_read_input_tokens": 0,
            "cache_creation_input_tokens": 0,
        },
    }

    client = anthropic.Anthropic()
    messages = [{"role": "user", "content": case["task"]}]

    async with stdio_client(server_params) as (read, write):
        async with ClientSession(read, write) as session:
            await session.initialize()
            mcp_tools = await session.list_tools()
            anthropic_tools = mcp_tools_to_anthropic(mcp_tools)
            transcript["available_tools"] = [t["name"] for t in anthropic_tools]

            stop_reason = None
            for turn in range(max_turns):
                # Retry transient proxy failures: 502/503/504/timeouts AND
                # the proxy-specific "No available accounts" 422 (which is
                # a capacity error, not a real validation error).
                resp = None
                last_err = None
                for attempt in range(5):
                    try:
                        resp = client.messages.create(
                            model=model,
                            max_tokens=max_tokens,
                            system=SYSTEM_PROMPT,
                            tools=anthropic_tools,
                            messages=messages,
                        )
                        break
                    except (anthropic.InternalServerError,
                            anthropic.APITimeoutError,
                            anthropic.APIConnectionError) as e:
                        last_err = e
                    except anthropic.UnprocessableEntityError as e:
                        msg = str(e).lower()
                        # Proxy capacity / transient errors that 422 — retry.
                        # Real validation errors (e.g. bad model id, malformed
                        # tool schema) shouldn't contain these phrases and
                        # will bubble out.
                        retryable_markers = (
                            "no available account",
                            "rate",
                            "upstream",
                            "unavailable",
                            "temporarily",
                            "timeout",
                        )
                        if not any(m in msg for m in retryable_markers):
                            raise
                        last_err = e
                    backoff = 5 * (2 ** attempt)  # 5s, 10s, 20s, 40s, 80s
                    if verbose:
                        print(f"  [retry {attempt+1}/5 after {backoff}s: "
                              f"{type(last_err).__name__}]")
                    await asyncio.sleep(backoff)
                if resp is None:
                    raise last_err if last_err else RuntimeError("messages.create failed silently")

                # Capture usage for this turn and add to running totals.
                u = getattr(resp, "usage", None)
                turn_usage = {
                    "input_tokens": getattr(u, "input_tokens", 0) or 0,
                    "output_tokens": getattr(u, "output_tokens", 0) or 0,
                    "cache_read_input_tokens": getattr(u, "cache_read_input_tokens", 0) or 0,
                    "cache_creation_input_tokens": getattr(u, "cache_creation_input_tokens", 0) or 0,
                }
                for k, v in turn_usage.items():
                    transcript["usage"][k] += v

                turn_record = {
                    "turn": turn,
                    "stop_reason": resp.stop_reason,
                    "usage": turn_usage,
                    "content": serializable_content(resp.content),
                    "results": [],
                }

                tool_uses = [b for b in resp.content if b.type == "tool_use"]
                messages.append({"role": "assistant", "content": resp.content})

                if verbose:
                    for b in resp.content:
                        if b.type == "text":
                            print(f"  [text] {b.text[:200]}")
                        elif b.type == "tool_use":
                            print(f"  [call] {b.name}({b.input})")

                if not tool_uses:
                    final_text = "\n".join(
                        b.text for b in resp.content if b.type == "text"
                    )
                    transcript["final_answer"] = final_text
                    transcript["turns"].append(turn_record)
                    stop_reason = resp.stop_reason
                    break

                tool_result_blocks = []
                for tu in tool_uses:
                    transcript["tool_calls"].append({
                        "turn": turn,
                        "name": tu.name,
                        "input": tu.input,
                    })
                    try:
                        result = await session.call_tool(tu.name, tu.input or {})
                        result_blocks = mcp_result_to_anthropic_blocks(result)
                        is_error = bool(getattr(result, "isError", False))
                    except Exception as e:
                        result_blocks = [{"type": "text", "text": f"Tool error: {e}"}]
                        is_error = True

                    log_text = stringify_for_log(result_blocks)
                    if verbose:
                        head = log_text[:200].replace("\n", " ")
                        print(f"  [result {'ERR' if is_error else 'OK'}] {head}")

                    turn_record["results"].append({
                        "tool_use_id": tu.id,
                        "name": tu.name,
                        "is_error": is_error,
                        "content": log_text[:4000],
                    })
                    tool_result_blocks.append({
                        "type": "tool_result",
                        "tool_use_id": tu.id,
                        "content": result_blocks,
                        "is_error": is_error,
                    })

                messages.append({"role": "user", "content": tool_result_blocks})
                transcript["turns"].append(turn_record)
            else:
                stop_reason = "max_turns_exhausted"

            transcript["stop"] = stop_reason
            transcript["n_turns"] = len(transcript["turns"])
            transcript["n_tool_calls"] = len(transcript["tool_calls"])

    out_path = out_dir / f"transcript_case_{case['case_id']:03d}_{case['needle']}.json"
    with open(out_path, "w") as f:
        json.dump(transcript, f, indent=2, default=str)
    u = transcript["usage"]
    print(f"  -> wrote {out_path}  (turns={transcript['n_turns']}, "
          f"tool_calls={transcript['n_tool_calls']}, stop={transcript['stop']})")
    print(f"     usage: input={u['input_tokens']}  output={u['output_tokens']}  "
          f"cache_read={u['cache_read_input_tokens']}  "
          f"cache_create={u['cache_creation_input_tokens']}")
    return transcript


async def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--manifest", type=Path,
                        default=REPO / "benchmark/cases/manifest.json")
    parser.add_argument("--out", type=Path,
                        default=REPO / "benchmark/transcripts")
    parser.add_argument("--case", type=int, default=None,
                        help="Run a single case_id (default: all)")
    parser.add_argument("--model", default="claude-sonnet-4-6",
                        help="Anthropic model id")
    parser.add_argument("--max-turns", type=int, default=25)
    parser.add_argument("--max-tokens", type=int, default=2048)
    parser.add_argument("--pv-host", default=os.environ.get("PV_HOST", "localhost"))
    parser.add_argument("--pv-port", type=int,
                        default=int(os.environ.get("PV_PORT", "11111")))
    parser.add_argument("--pvpython", default=os.environ.get("PVPYTHON", DEFAULT_PVPYTHON))
    parser.add_argument("--mcp-server", type=Path, default=MCP_SERVER_PATH)
    parser.add_argument("--verbose", action="store_true")
    args = parser.parse_args()

    if not (os.environ.get("ANTHROPIC_API_KEY") or os.environ.get("ANTHROPIC_AUTH_TOKEN")):
        print("ERROR: neither ANTHROPIC_API_KEY nor ANTHROPIC_AUTH_TOKEN is set.",
              file=sys.stderr)
        sys.exit(2)

    args.out.mkdir(parents=True, exist_ok=True)
    with open(args.manifest) as f:
        manifest = json.load(f)

    if args.case is not None:
        cases = [c for c in manifest if c["case_id"] == args.case]
        if not cases:
            print(f"No case with id {args.case}", file=sys.stderr)
            sys.exit(1)
    else:
        cases = manifest

    for case in cases:
        print(f"[case {case['case_id']:03d} {case['needle']}] {case['task'][:80]}...")
        try:
            await run_case(
                case=case,
                model=args.model,
                max_turns=args.max_turns,
                max_tokens=args.max_tokens,
                pv_host=args.pv_host,
                pv_port=args.pv_port,
                pvpython=args.pvpython,
                mcp_server_path=args.mcp_server,
                out_dir=args.out,
                verbose=args.verbose,
            )
        except Exception as e:
            print(f"  !! case {case['case_id']} crashed: {e}")
            import traceback; traceback.print_exc()


if __name__ == "__main__":
    asyncio.run(main())
