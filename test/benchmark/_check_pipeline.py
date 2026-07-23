"""
End-to-end MCP smoke test, no LLM involved.

Spawns the MCP server, loads a real generated case, and exercises a few
tools to confirm pvserver + paraview_manager + the new get_histogram all
work against synthetic data.
"""
import io, os, sys
_real_stdin = os.fdopen(os.dup(0), "rb", buffering=0)
_real_stdout = os.fdopen(os.dup(1), "wb", buffering=0)
_real_stderr = os.fdopen(os.dup(2), "wb", buffering=0)
sys.stdin = io.TextIOWrapper(_real_stdin, encoding="utf-8")
sys.stdout = io.TextIOWrapper(_real_stdout, encoding="utf-8", write_through=True)
sys.stderr = io.TextIOWrapper(_real_stderr, encoding="utf-8", write_through=True)

import asyncio, json
from pathlib import Path
from mcp import ClientSession, StdioServerParameters
from mcp.client.stdio import stdio_client

REPO = Path(__file__).resolve().parent.parent
PVPYTHON = ("/mnt/common/hxu40/spack/opt/spack/linux-skylake_avx512/"
            "paraview-5.13.3-ssmv5hp4czyfvuu5eps6s2ljpug7lkus/bin/pvpython")
CASE_VTI = REPO / "benchmark/cases/case_000_hotspot.vti"
TRUTH = REPO / "benchmark/cases/case_000_hotspot.truth.json"


def text_of(call_result):
    parts = []
    for item in call_result.content:
        if hasattr(item, "text") and item.text is not None:
            parts.append(item.text)
    return "\n".join(parts)


async def main():
    with open(TRUTH) as f:
        truth = json.load(f)
    print("=== ground truth ===")
    print(json.dumps(truth, indent=2))
    print()

    params = StdioServerParameters(
        command=PVPYTHON,
        args=[str(REPO / "paraview_mcp_server.py"),
              "--server", "localhost", "--port", "11111"],
        env=os.environ.copy(),
    )
    async with stdio_client(params) as (r, w):
        async with ClientSession(r, w) as session:
            await session.initialize()

            print("=== load_data ===")
            res = await session.call_tool("load_data", {"file_path": str(CASE_VTI)})
            print(text_of(res))
            print()

            print("=== get_available_arrays ===")
            res = await session.call_tool("get_available_arrays", {})
            print(text_of(res))
            print()

            print("=== get_histogram (Temperature, 16 bins) ===")
            res = await session.call_tool(
                "get_histogram",
                {"field": "Temperature", "num_bins": 16, "data_location": "POINTS"},
            )
            print(text_of(res))
            print()

            print("=== get_pipeline ===")
            res = await session.call_tool("get_pipeline", {})
            print(text_of(res))


asyncio.run(main())
