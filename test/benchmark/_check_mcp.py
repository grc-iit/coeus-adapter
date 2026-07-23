"""One-shot sanity check: spawn the MCP server and list its tools."""
import io, os, sys
_real_stdout = os.fdopen(os.dup(1), "wb", buffering=0)
_real_stderr = os.fdopen(os.dup(2), "wb", buffering=0)
sys.stdout = io.TextIOWrapper(_real_stdout, write_through=True)
sys.stderr = io.TextIOWrapper(_real_stderr, write_through=True)

import asyncio
from pathlib import Path
from mcp import ClientSession, StdioServerParameters
from mcp.client.stdio import stdio_client

REPO = Path(__file__).resolve().parent.parent
PVPYTHON = (
    "/mnt/common/hxu40/spack/opt/spack/linux-skylake_avx512/"
    "paraview-5.13.3-ssmv5hp4czyfvuu5eps6s2ljpug7lkus/bin/pvpython"
)

async def main():
    params = StdioServerParameters(
        command=PVPYTHON,
        args=[str(REPO / "paraview_mcp_server.py"),
              "--server", "localhost", "--port", "11111"],
        env=os.environ.copy(),
    )
    async with stdio_client(params) as (r, w):
        async with ClientSession(r, w) as session:
            await session.initialize()
            tools = await session.list_tools()
            names = sorted(t.name for t in tools.tools)
            print(f"OK: {len(names)} tools")
            for n in names:
                print(" -", n)

asyncio.run(main())
