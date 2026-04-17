"""
In-Situ AI Agent for Gray-Scott Simulation

This is the agent that connects to the in-situ MCP server and uses an LLM
(OpenAI or Anthropic) to interactively explore live simulation data.

The agent:
1. Launches the MCP server as a subprocess (stdio transport)
2. Discovers available tools (streaming control + ParaView visualization)
3. Runs an LLM-driven loop: user prompt → LLM decides which tools to call →
   executes tools via MCP → feeds results back to LLM → repeat

Usage:
  # Make sure pvserver and insitu_streaming.py are already running, then:

  # With OpenAI:
  export OPENAI_API_KEY=sk-...
  python insitu_agent.py --provider openai --model gpt-4o

  # With Anthropic (Sonnet):
  export ANTHROPIC_API_KEY=sk-ant-...
  python insitu_agent.py --provider anthropic --model claude-sonnet-4-20250514

  # With Anthropic (Opus):
  python insitu_agent.py --provider anthropic --model claude-opus-4-20250514

  # Interactive mode (default): type prompts, agent responds with tool calls
  # Single-shot mode: pass a prompt directly
  python insitu_agent.py --provider openai --prompt "Show me an isosurface of V at 0.5"
"""

import asyncio
import argparse
import base64
import json
import os
import sys
import time
from pathlib import Path

from mcp import ClientSession, StdioServerParameters
from mcp.client.stdio import stdio_client


# Module-level run statistics — accumulated across all LLM calls + MCP tool calls
_run_stats = {
    "input_tokens": 0,
    "output_tokens": 0,
    "cache_creation_input_tokens": 0,
    "cache_read_input_tokens": 0,
    "num_llm_calls": 0,
    "num_tool_calls": 0,
    "num_screenshots": 0,
    "total_image_bytes": 0,
    "model": None,
    "provider": None,
    "wall_start": None,
}

# Optional results directory; when set, screenshot bytes are persisted to disk.
_results_dir = None


def _save_screenshot_bytes(b64_data):
    """Decode base64 image bytes from MCP and write to results_dir/screenshots/."""
    global _results_dir
    if not _results_dir:
        return None, 0
    try:
        img_bytes = base64.b64decode(b64_data)
    except Exception:
        return None, 0
    screenshots_dir = Path(_results_dir) / "screenshots"
    screenshots_dir.mkdir(parents=True, exist_ok=True)
    idx = _run_stats["num_screenshots"] + 1
    out = screenshots_dir / f"agent_{idx:04d}.png"
    out.write_bytes(img_bytes)
    return str(out), len(img_bytes)


SYSTEM_PROMPT = """\
You are an AI agent controlling a live in-situ scientific visualization of a \
Gray-Scott reaction-diffusion simulation. Data streams in real-time via ADIOS2 SST \
and you interact with it through ParaView.

Available fields: U (reactant), V (product — usually more interesting).

Workflow:
1. Check streaming status first (get_streaming_status).
2. Pause the stream to explore a timestep in detail (pause_streaming).
3. Create visualizations: isosurfaces, slices, volume rendering.
4. Take screenshots to see results (get_screenshot).
5. Advance timesteps to watch the simulation evolve (advance_step).

Be concise. Call only the tools you need. When you take a screenshot, describe \
what you see and suggest next steps.\
"""


# Hard caps so a misbehaving run doesn't burn tokens forever. Tunable by
# the run script via --max-iterations / --max-wall-seconds.
_run_limits = {
    "max_iterations": 200,
    "max_wall_seconds": 600.0,
    "iterations": 0,
}


async def run_agent_loop(session, tools, provider, model, user_messages):
    """Run the LLM ↔ MCP tool-calling loop."""
    tool_schemas = _build_tool_schemas(tools, provider)

    messages = [{"role": "system", "content": SYSTEM_PROMPT}]
    messages.extend(user_messages)

    while True:
        # Hard safety caps — bail if the loop runs too long or spins forever
        _run_limits["iterations"] += 1
        if _run_limits["iterations"] > _run_limits["max_iterations"]:
            print(f"\n[insitu_agent] SAFETY STOP: max_iterations={_run_limits['max_iterations']} reached")
            break
        if _run_stats["wall_start"] is not None:
            elapsed = time.monotonic() - _run_stats["wall_start"]
            if elapsed > _run_limits["max_wall_seconds"]:
                print(f"\n[insitu_agent] SAFETY STOP: max_wall_seconds={_run_limits['max_wall_seconds']:.0f}s reached ({elapsed:.0f}s elapsed)")
                break

        if provider == "openai":
            response = await _call_openai(messages, tool_schemas, model)
        else:
            response = await _call_anthropic(messages, tool_schemas, model)

        assistant_msg, tool_calls = response
        messages.append(assistant_msg)

        if assistant_msg.get("content"):
            print(f"\n🤖 Agent: {assistant_msg['content']}\n")

        if not tool_calls:
            break

        for tc in tool_calls:
            tool_name = tc["name"]
            tool_args = tc["arguments"]
            call_id = tc["id"]

            print(f"  🔧 Calling: {tool_name}({json.dumps(tool_args)})")

            result = await session.call_tool(tool_name, arguments=tool_args)
            _run_stats["num_tool_calls"] += 1

            result_text = ""
            result_image_blocks = []
            for block in result.content:
                if hasattr(block, "text") and block.text:
                    result_text += block.text
                elif hasattr(block, "data") and block.data:
                    saved_path, n_bytes = _save_screenshot_bytes(block.data)
                    _run_stats["num_screenshots"] += 1
                    _run_stats["total_image_bytes"] += n_bytes
                    if saved_path:
                        result_text += f"[Image: {n_bytes} bytes saved to {Path(saved_path).name}]"
                    else:
                        result_text += f"[Image: {n_bytes} bytes]"
                    # Pass the actual image back to the model so it can "see" it
                    result_image_blocks.append({
                        "type": "image",
                        "source": {
                            "type": "base64",
                            "media_type": "image/png",
                            "data": block.data,
                        },
                    })

            print(f"  ✅ Result: {result_text[:200]}{'...' if len(result_text) > 200 else ''}")

            if provider == "openai":
                messages.append({
                    "role": "tool",
                    "tool_call_id": call_id,
                    "content": result_text,
                })
            else:
                # Anthropic tool_result content can be a list of blocks
                # mixing text and image (vision-enabled models).
                tr_content = [{"type": "text", "text": result_text or "(no text)"}]
                tr_content.extend(result_image_blocks)
                messages.append({
                    "role": "user",
                    "content": [
                        {
                            "type": "tool_result",
                            "tool_use_id": call_id,
                            "content": tr_content,
                        }
                    ],
                })

    return messages


def _build_tool_schemas(tools, provider):
    """Convert MCP tool definitions to the LLM provider's tool schema format."""
    schemas = []
    for tool in tools:
        if provider == "openai":
            schemas.append({
                "type": "function",
                "function": {
                    "name": tool.name,
                    "description": tool.description or "",
                    "parameters": tool.inputSchema if tool.inputSchema else {"type": "object", "properties": {}},
                },
            })
        else:
            schemas.append({
                "name": tool.name,
                "description": tool.description or "",
                "input_schema": tool.inputSchema if tool.inputSchema else {"type": "object", "properties": {}},
            })
    return schemas


async def _call_openai(messages, tools, model):
    """Call OpenAI chat completions with tool use."""
    import openai

    client = openai.AsyncOpenAI()

    response = await client.chat.completions.create(
        model=model,
        messages=messages,
        tools=tools if tools else None,
    )

    choice = response.choices[0]
    msg = choice.message

    assistant_msg = {"role": "assistant", "content": msg.content or ""}

    tool_calls = []
    if msg.tool_calls:
        assistant_msg["tool_calls"] = [
            {
                "id": tc.id,
                "type": "function",
                "function": {"name": tc.function.name, "arguments": tc.function.arguments},
            }
            for tc in msg.tool_calls
        ]
        for tc in msg.tool_calls:
            tool_calls.append({
                "id": tc.id,
                "name": tc.function.name,
                "arguments": json.loads(tc.function.arguments),
            })

    return assistant_msg, tool_calls


async def _call_anthropic(messages, tools, model):
    """Call Anthropic messages API with tool use."""
    import anthropic

    client = anthropic.AsyncAnthropic()

    system_msg = None
    api_messages = []
    for m in messages:
        if m["role"] == "system":
            system_msg = m["content"]
        else:
            api_messages.append(m)

    kwargs = {
        "model": model,
        "max_tokens": 4096,
        "messages": api_messages,
    }
    if system_msg:
        kwargs["system"] = system_msg
    if tools:
        kwargs["tools"] = tools

    response = await client.messages.create(**kwargs)

    # Accumulate token usage
    _run_stats["num_llm_calls"] += 1
    if hasattr(response, "usage") and response.usage is not None:
        _run_stats["input_tokens"] += getattr(response.usage, "input_tokens", 0) or 0
        _run_stats["output_tokens"] += getattr(response.usage, "output_tokens", 0) or 0
        _run_stats["cache_creation_input_tokens"] += getattr(response.usage, "cache_creation_input_tokens", 0) or 0
        _run_stats["cache_read_input_tokens"] += getattr(response.usage, "cache_read_input_tokens", 0) or 0

    content_text = ""
    tool_calls = []
    tool_use_blocks = []

    for block in response.content:
        if block.type == "text":
            content_text += block.text
        elif block.type == "tool_use":
            tool_calls.append({
                "id": block.id,
                "name": block.name,
                "arguments": block.input,
            })
            tool_use_blocks.append({
                "type": "tool_use",
                "id": block.id,
                "name": block.name,
                "input": block.input,
            })

    # Anthropic's API rejects empty text blocks (the proxy used to filter them
    # silently). Only emit a text block when the model actually produced text.
    if tool_use_blocks:
        content_blocks = []
        if content_text:
            content_blocks.append({"type": "text", "text": content_text})
        content_blocks.extend(tool_use_blocks)
        assistant_msg = {"role": "assistant", "content": content_blocks}
    else:
        assistant_msg = {"role": "assistant", "content": content_text or " "}

    return assistant_msg, tool_calls


async def interactive_loop(session, tools, provider, model):
    """Run an interactive REPL where the user types prompts."""
    print("\n" + "=" * 60)
    print("  In-Situ AI Agent — Interactive Mode")
    print("  Type your prompts. Type 'quit' or 'exit' to stop.")
    print("=" * 60)

    conversation = []

    while True:
        try:
            user_input = input("\n👤 You: ").strip()
        except (EOFError, KeyboardInterrupt):
            print("\nExiting.")
            break

        if user_input.lower() in ("quit", "exit", "q"):
            print("Exiting.")
            break

        if not user_input:
            continue

        conversation.append({"role": "user", "content": user_input})

        conversation = await run_agent_loop(
            session, tools, provider, model, conversation
        )


async def main():
    parser = argparse.ArgumentParser(description="In-Situ AI Agent")
    parser.add_argument(
        "--provider", choices=["openai", "anthropic"], default="openai",
        help="LLM provider (default: openai)",
    )
    parser.add_argument(
        "--model", type=str, default=None,
        help="Model name (default: gpt-4o for openai, claude-sonnet-4-20250514 for anthropic)",
    )
    parser.add_argument(
        "--prompt", type=str, default=None,
        help="Single-shot prompt (if omitted, runs interactive mode)",
    )
    parser.add_argument(
        "--server-host", type=str, default="localhost",
        help="pvserver hostname (default: localhost)",
    )
    parser.add_argument(
        "--server-port", type=int, default=11112,
        help="pvserver port (default: 11112)",
    )
    parser.add_argument(
        "--status-file", type=str, default=None,
        help="Path to streaming_status.json (default: auto-detect in script dir)",
    )
    parser.add_argument(
        "--paraview-package-path", type=str, default=None,
        help="Path to the ParaView Python package",
    )
    parser.add_argument(
        "--pvpython", type=str, default=None,
        help="Path to pvpython binary for MCP server (default: sys.executable)",
    )
    parser.add_argument(
        "--timing-file", type=str, default=None,
        help="Path to write per-tool timing JSONL (passed to MCP server)",
    )
    parser.add_argument(
        "--screenshot-file", type=str, default=None,
        help="Path to the bridge-saved screenshot PNG (passed to MCP server)",
    )
    parser.add_argument(
        "--results-dir", type=str, default=None,
        help="Directory to save per-run artifacts (screenshots, token_usage.json)",
    )
    parser.add_argument(
        "--prompt-file", type=str, default=None,
        help="Read the prompt from a file (overrides --prompt)",
    )
    parser.add_argument(
        "--max-iterations", type=int, default=200,
        help="Max LLM tool-calling iterations (safety cap)",
    )
    parser.add_argument(
        "--max-wall-seconds", type=float, default=600.0,
        help="Max agent-loop wall time in seconds (safety cap)",
    )

    args = parser.parse_args()

    if args.model is None:
        args.model = "gpt-4o" if args.provider == "openai" else "claude-sonnet-4-20250514"

    # Configure global run-stats sink
    global _results_dir
    _run_stats["model"] = args.model
    _run_stats["provider"] = args.provider
    _run_stats["wall_start"] = time.monotonic()
    if args.results_dir:
        _results_dir = args.results_dir
        Path(_results_dir).mkdir(parents=True, exist_ok=True)

    _run_limits["max_iterations"] = args.max_iterations
    _run_limits["max_wall_seconds"] = args.max_wall_seconds
    _run_limits["iterations"] = 0

    if args.prompt_file:
        with open(args.prompt_file, "r") as f:
            args.prompt = f.read()

    script_dir = Path(__file__).resolve().parent
    status_file = args.status_file or str(script_dir / "streaming_status.json")

    mcp_server_script = str(script_dir / "insitu_mcp_server.py")
    mcp_args = [
        mcp_server_script,
        "--server", args.server_host,
        "--port", str(args.server_port),
        "--status-file", status_file,
    ]
    if args.paraview_package_path:
        mcp_args.extend(["--paraview_package_path", args.paraview_package_path])
    if args.timing_file:
        mcp_args.extend(["--timing-file", args.timing_file])
    if args.screenshot_file:
        mcp_args.extend(["--screenshot-file", args.screenshot_file])

    mcp_command = args.pvpython or sys.executable

    server_params = StdioServerParameters(
        command=mcp_command,
        args=mcp_args,
        env={**os.environ},
    )

    print(f"Launching MCP server: {mcp_command} {' '.join(mcp_args)}")
    print(f"Using LLM: {args.provider}/{args.model}")

    async with stdio_client(server_params) as (read, write):
        async with ClientSession(read, write) as session:
            await session.initialize()

            tools_response = await session.list_tools()
            tools = tools_response.tools

            print(f"\nDiscovered {len(tools)} MCP tools:")
            for t in tools:
                print(f"  - {t.name}: {t.description[:60] if t.description else ''}...")

            if args.prompt:
                user_messages = [{"role": "user", "content": args.prompt}]
                await run_agent_loop(session, tools, args.provider, args.model, user_messages)
            else:
                await interactive_loop(session, tools, args.provider, args.model)

    # Write final token usage / run summary
    if args.results_dir:
        wall_total = time.monotonic() - _run_stats["wall_start"]
        # Anthropic April 2026 pricing per million tokens
        pricing = {
            "claude-haiku-4-5-20251001": {"in": 1.00, "out": 5.00},
            "claude-haiku-4-5":          {"in": 1.00, "out": 5.00},
            "claude-sonnet-4-5-20250929":{"in": 3.00, "out": 15.00},
            "claude-sonnet-4-5":         {"in": 3.00, "out": 15.00},
            "claude-opus-4-20250514":    {"in": 15.00, "out": 75.00},
        }
        rates = pricing.get(args.model, {"in": 0.0, "out": 0.0})
        cost = (
            _run_stats["input_tokens"] * rates["in"] / 1e6
            + _run_stats["output_tokens"] * rates["out"] / 1e6
        )
        summary = {
            **_run_stats,
            "wall_time_s": round(wall_total, 3),
            "estimated_cost_usd": round(cost, 4),
            "pricing_per_million": rates,
        }
        # Drop unserializable wall_start
        summary.pop("wall_start", None)
        out = Path(args.results_dir) / "token_usage.json"
        out.write_text(json.dumps(summary, indent=2))
        print(f"\n[insitu_agent] saved token usage → {out}")
        print(json.dumps(summary, indent=2))


if __name__ == "__main__":
    asyncio.run(main())
