# In-Situ AI Agent for Gray-Scott Simulation

Interactive AI agent that uses ParaView MCP tools to explore live Gray-Scott
simulation data streamed via ADIOS2 SST.

## Architecture

```
┌──────────────────────┐       ADIOS2 SST        ┌─────────────────────────┐
│  Gray-Scott Sim      │ ──── "gs.bp" ─────────►  │  insitu_streaming.py    │
│  (adios2-gray-scott) │                           │  (pvpython on pvserver) │
│                      │  gs.bp.sst contact file   │                         │
│  BeginStep/EndStep   │                           │  Fides reader → pvserver│
└──────────────────────┘                           └────────────┬────────────┘
                                                                │
                                                         pvserver (port 11111)
                                                         --multi-clients
                                                                │
                                                   ┌────────────┴────────────┐
                                                   │                         │
                                            ┌──────┴──────┐          ┌──────┴──────┐
                                            │ ParaView GUI│          │ insitu_mcp  │
                                            │ (optional)  │          │ _server.py  │
                                            └─────────────┘          └──────┬──────┘
                                                                            │
                                                                      MCP Protocol
                                                                            │
                                                                     ┌──────┴──────┐
                                                                     │  AI Agent   │
                                                                     │ (Cursor/    │
                                                                     │  Claude)    │
                                                                     └─────────────┘
```

## Prerequisites

- ParaView with pvserver and pvpython (conda install conda-forge::paraview)
- ADIOS2 with SST support
- Gray-Scott simulation binary (adios2-gray-scott)
- Python packages: `mcp[cli]`, `httpx`

## Quick Start

### Step 1: Install dependencies

```bash
pip install -r requirements.txt
```

### Step 2: Start pvserver

```bash
pvserver --multi-clients --server-port=11111
```

### Step 3: (Optional) Connect ParaView GUI

Open ParaView GUI → File → Connect → localhost:11111.
This lets you see the visualization live alongside the AI agent.

### Step 4: Start the Gray-Scott simulation

```bash
cd test/insitu_agent
mpirun -n 4 adios2-gray-scott settings-staging.json
```

This writes to the SST stream `gs.bp` using the config in `adios2-sst.xml`.

### Step 5: Start the streaming bridge

```bash
pvpython insitu_streaming.py \
    -j gs-fides.json \
    -b gs.bp \
    --staging \
    --server localhost \
    --port 11111 \
    --paused
```

The `--paused` flag starts in paused mode so the AI agent controls when to
advance timesteps. Remove it for auto-advance mode (2s delay between steps).

### Step 6: Run the AI agent

The agent launches the MCP server internally — you don't need to start it separately.

```bash
# With OpenAI
export OPENAI_API_KEY=sk-...
python insitu_agent.py --provider openai --model gpt-4o

# With Anthropic
export ANTHROPIC_API_KEY=sk-ant-...
python insitu_agent.py --provider anthropic --model claude-sonnet-4-20250514

# Single-shot mode (one prompt, no interactive loop)
python insitu_agent.py --provider openai --prompt "Create an isosurface of V at 0.5 and take a screenshot"
```

The agent will:
1. Launch `insitu_mcp_server.py` as a subprocess (stdio MCP transport)
2. Connect to the same pvserver where the streaming bridge is running
3. Discover all available tools (streaming control + ParaView visualization)
4. Enter an interactive loop where you type prompts and the LLM calls tools

### Alternative: Use the MCP server with Cursor / Claude Desktop

Instead of `insitu_agent.py`, you can connect the MCP server to any MCP-compatible
client. Add to your MCP configuration (e.g., Cursor `mcp.json` or Claude Desktop config):

```json
{
  "mcpServers": {
    "InSitu-ParaView": {
      "command": "python",
      "args": [
        "/path/to/test/insitu_agent/insitu_mcp_server.py",
        "--server", "localhost",
        "--port", "11111",
        "--status-file", "/path/to/test/insitu_agent/streaming_status.json"
      ]
    }
  }
}
```

## Available MCP Tools

### Streaming Control
| Tool | Description |
|------|-------------|
| `get_streaming_status` | Check current timestep, pause state, stream status |
| `pause_streaming` | Pause the stream to explore current data |
| `resume_streaming` | Resume auto-advancing through timesteps |
| `advance_step` | Advance exactly one timestep, then pause |

### Visualization
| Tool | Description |
|------|-------------|
| `create_isosurface` | Create isosurface on live data (field, value) |
| `create_slice` | Slice through the volume at any plane |
| `toggle_volume_rendering` | Enable/disable volume rendering |
| `color_by` | Color by field (U or V) |
| `set_color_map` | Custom color transfer function |
| `edit_volume_opacity` | Custom opacity transfer function |
| `create_streamline` | Streamline visualization |

### Inspection
| Tool | Description |
|------|-------------|
| `get_screenshot` | Capture current view as image |
| `get_pipeline` | Show pipeline structure |
| `get_available_arrays` | List data arrays (U, V) |
| `compute_surface_area` | Compute surface area of active mesh |

## Example Agent Session

```
$ python insitu_agent.py --provider openai --model gpt-4o

Launching MCP server: python insitu_mcp_server.py --server localhost --port 11111 ...
Using LLM: openai/gpt-4o

Discovered 24 MCP tools:
  - get_streaming_status: Get the current status of the in-situ streaming...
  - pause_streaming: Pause the streaming pipeline...
  - advance_step: Advance the stream by exactly one timestep...
  - create_isosurface: Create an isosurface visualization on the live...
  - create_slice: Create a slice through the live simulation volume...
  - get_screenshot: Capture a screenshot of the current view...
  ...

============================================================
  In-Situ AI Agent — Interactive Mode
  Type your prompts. Type 'quit' or 'exit' to stop.
============================================================

👤 You: Check the status and show me what the data looks like

  🔧 Calling: get_streaming_status({})
  ✅ Result: Streaming status: Current timestep: 0, Paused: true...
  🔧 Calling: advance_step({})
  ✅ Result: Advance command sent. The stream will read one step and pause.
  🔧 Calling: get_screenshot({})
  ✅ Result: [Image: 45231 bytes]

🤖 Agent: The simulation is at timestep 1. I can see the initial volume data
   colored by V. The domain shows early-stage pattern formation. Would you like
   me to create an isosurface to highlight the reaction fronts?

👤 You: Yes, show me isosurfaces of V at 0.3 and 0.5

  🔧 Calling: create_isosurface({"value": 0.3, "field": "V"})
  ✅ Result: Created isosurface at value 0.3. Filter registered as 'Contour1'.
  🔧 Calling: get_screenshot({})
  ✅ Result: [Image: 52108 bytes]

🤖 Agent: I've created an isosurface at V=0.3 showing the outer boundary of
   the reaction zone. Let me advance a few steps to see evolution...

👤 You: Advance 5 steps and take a screenshot after each

  🔧 Calling: advance_step({})
  🔧 Calling: get_screenshot({})
  ... (repeats 5 times)

🤖 Agent: The pattern is clearly evolving — the V=0.3 isosurface shows
   expanding spots that are beginning to merge. Want me to add a slice
   to see the interior structure?

👤 You: quit
Exiting.
```

## Files

| File | Purpose |
|------|---------|
| `insitu_agent.py` | **AI agent** — LLM loop that calls MCP tools interactively |
| `insitu_mcp_server.py` | **MCP server** — streaming control + ParaView tools via MCP |
| `insitu_streaming.py` | **Streaming bridge** — reads SST data into pvserver via Fides |
| `adios2-sst.xml` | ADIOS2 config: SST engine with QueueLimit=3, Discard policy |
| `gs-fides.json` | Fides data model: maps U, V arrays to VTK Cartesian grid |
| `settings-staging.json` | Gray-Scott simulation settings pointing to SST config |
| `requirements.txt` | Python dependencies |
| `streaming_status.json` | Runtime: current step/pause state (auto-generated) |
| `streaming_command.json` | Runtime: MCP→bridge commands (auto-generated) |

## SST Tuning

Edit `adios2-sst.xml` to change the coupling behavior:

- **`QueueLimit`**: How many steps to buffer. Higher = more lag tolerance for the agent.
- **`QueueFullPolicy`**: `Discard` (drop old data, non-blocking sim) or `Block` (sim waits).
- **`RendezvousReaderCount`**: `1` = sim waits for reader; `0` = sim starts immediately.
- **`DataTransport`**: `WAN` for TCP (cross-node), `MPI` for same-node.
