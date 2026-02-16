# IOWarp CTE MCP Server

Model Context Protocol (MCP) server wrapping the Context Interface Python APIs.

## Overview

This MCP server exposes all Context Interface operations (context_bundle, context_query, context_delete) plus additional CTE functionality through standardized tools:

### Context Interface Operations

The core Context Interface operations as defined in `test_bindings.py`:

- **put_blob**: Store data in blobs under tags (context_bundle)
  - Creates or gets a tag by name
  - Stores blob data under the tag
  
- **list_blobs_in_tag**: List all blob names in a tag (context_query - list)
  - Returns list of blob names contained in a tag
  
- **get_blob_size**: Get the size of a blob (context_query - get size)
  - Returns blob size in bytes
  
- **get_blob**: Retrieve blob data from a tag (context_query - get data)
  - Returns blob data as string or base64
  
- **delete_blob**: Delete a blob from a tag (context_delete)
  - Removes a blob from a tag

### Additional CTE Operations

- **tag_query**: Query tags by regex pattern
- **blob_query**: Query blobs by tag and blob regex patterns  
- **poll_telemetry_log**: Poll telemetry log with time filter
- **reorganize_blob**: Reorganize blob placement with new score

### Runtime Management

- **initialize_cte_runtime**: Initialize CTE runtime (Chimaera runtime, client, and CTE subsystem)
- **get_client_status**: Check CTE client initialization status
- **get_cte_types**: Discover available CTE types and operations

## Prerequisites

- Python 3.8 or higher
- CTE Python bindings (`wrp_cte_core_ext`) must be built (see Building section)
- PyYAML (for automatic config generation) - installed via requirements.txt
- **Note**: CTE runtime must be initialized for all operations to work - use `initialize_cte_runtime` tool first

## Quick Start

### 1. Setup Virtual Environment

```bash
cd context-exploration-engine/iowarp-cei-mcp
python3 -m venv venv
source venv/bin/activate  # On Windows: venv\Scripts\activate
```

### 2. Install Dependencies

```bash
pip install -r requirements.txt
```

### 3. Set PYTHONPATH (if CTE bindings not in system path)

```bash
# Point to where CTE Python bindings are built
export PYTHONPATH=/workspace/build/bin:$PYTHONPATH
```

### 4. Run Tests

```bash
python3 test_mcp_tools.py
```

This test suite mimics MCP behavior by using the MCP client library to call all tools
and capture their inputs and outputs. It tests all Context Interface operations plus
additional CTE operations.

**Expected Output:**
- Tests for safe functions (`get_client_status`, `get_cte_types`) should pass
- Runtime-dependent functions may show "Expected Failure" if runtime is not initialized
- After running `initialize_cte_runtime`, all Context Interface operations should work

### Test Individual Tools

You can test individual tools programmatically using the test file:

```python
# See test_mcp_tools.py for examples of how to test each tool
# The test file mimics MCP behavior and shows input/output for all tools
```

## Building CTE Python Bindings

If CTE Python bindings are not available:

### Step 1: Build CTE with Python Bindings

```bash
cd /workspace
mkdir -p build && cd build
cmake .. -DWITH_PYTHON_BINDINGS=ON
make -j$(nproc)
```

### Step 2: Set PYTHONPATH

```bash
export PYTHONPATH=/workspace/build/bin:$PYTHONPATH
```

Or add to your shell profile:
```bash
echo 'export PYTHONPATH=/workspace/build/bin:$PYTHONPATH' >> ~/.bashrc
source ~/.bashrc
```

### Step 3: Verify

```bash
python3 -c "import wrp_cte_core_ext as cte; print('✅ CTE bindings available')"
```

## Important: CTE Runtime Initialization

**CRITICAL**: Query and reorganization functions (`tag_query`, `blob_query`, 
`poll_telemetry_log`, `reorganize_blob`) require CTE runtime to be initialized 
before use. Without runtime initialization, these functions will cause a 
segmentation fault that crashes the server process.

**Safe functions** (always work):
- `get_client_status` - Check initialization status
- `get_cte_types` - Get available types

**Runtime-dependent functions** (require initialization):
- `tag_query` - Query tags
- `blob_query` - Query blobs  
- `poll_telemetry_log` - Poll telemetry
- `reorganize_blob` - Reorganize blobs

## Usage with MCP Clients

### Gemini CLI

To test with Gemini CLI, see [GEMINI_CLI_SETUP.md](GEMINI_CLI_SETUP.md) for detailed instructions.

Quick start:
1. Install Gemini CLI: `npm install -g @google/gemini-cli`
2. Start Gemini CLI: `gemini`
3. Use `/mcp` command to connect to your MCP server
4. Configure server path in Gemini CLI config

For full setup instructions, see [GEMINI_CLI_SETUP.md](GEMINI_CLI_SETUP.md).

### Claude Desktop

Add to `claude_desktop_config.json`:

```json
{
  "mcpServers": {
    "iowarp-cte": {
      "command": "python3",
      "args": ["/full/path/to/context-exploration-engine/iowarp-cei-mcp/server.py"],
      "env": {
        "PYTHONPATH": "/workspace/build/bin"
      }
    }
  }
}
```

### Programmatic Usage

```python
from mcp import ClientSession, StdioServerParameters
from mcp.client.stdio import stdio_client
from pathlib import Path

async def use_mcp():
    server_script = Path("server.py").absolute()
    server_params = StdioServerParameters(
        command="python3",
        args=[str(server_script)],
        env={"PYTHONPATH": "/workspace/build/bin"}
    )
    
    async with stdio_client(server_params) as (read, write):
        async with ClientSession(read, write) as session:
            await session.initialize()
            
            # List available tools
            tools = await session.list_tools()
            print(f"Available tools: {[t.name for t in tools.tools]}")
            
            # Call a tool
            result = await session.call_tool("get_client_status", {})
            print(result.content[0].text)

# Run it
import asyncio
asyncio.run(use_mcp())
```

## Troubleshooting

### "CTE Python bindings not available"

- Set `PYTHONPATH` to point to build directory: `export PYTHONPATH=/workspace/build/bin:$PYTHONPATH`
- Verify bindings exist: `ls /workspace/build/bin/wrp_cte_core_ext*.so`
- Rebuild bindings if missing (see Building section)

### "Connection closed" errors in tests

- This happens when query functions are called without CTE runtime initialization
- This is expected behavior - query functions require runtime
- Safe functions (`get_client_status`, `get_cte_types`) will still work

### Import errors

- Make sure virtual environment is activated: `source venv/bin/activate`
- Reinstall dependencies: `pip install -r requirements.txt`
- Check Python version: `python3 --version` (needs 3.8+)

## File Structure

```
iowarp-cei-mcp/
├── server.py                    # MCP server implementation
├── test_mcp_tools.py            # Test suite that mimics MCP behavior
├── requirements.txt             # Python dependencies
└── README.md                    # This file
```

## License

Part of the IOWarp project.
