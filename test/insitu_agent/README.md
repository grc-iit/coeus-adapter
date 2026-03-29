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

### Step 1: Start pvserver

```bash
pvserver --multi-clients --server-port=11111
```

### Step 2: (Optional) Connect ParaView GUI

Open ParaView GUI → File → Connect → localhost:11111.
This lets you see the visualization live alongside the AI agent.

### Step 3: Start the Gray-Scott simulation

```bash
cd test/insitu_agent
mpirun -n 4 adios2-gray-scott settings-staging.json
```

This writes to the SST stream `gs.bp` using the config in `adios2-sst.xml`.

### Step 4: Start the streaming bridge

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

### Step 5: Start the MCP server

```bash
python insitu_mcp_server.py \
    --server localhost \
    --port 11111 \
    --status-file streaming_status.json
```

### Step 6: Configure your AI agent

Add to your MCP configuration (e.g., Cursor `mcp.json` or Claude Desktop config):

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

## Example AI Agent Interaction

```
Agent: Let me check the streaming status.
→ calls get_streaming_status()
← "Current timestep: 0, Paused: true, Pipeline ready: true"

Agent: I'll advance to the first timestep and take a look.
→ calls advance_step()
→ calls get_screenshot()
← [image of raw volume data]

Agent: Let me create an isosurface of the V field at 0.5.
→ calls create_isosurface(value=0.5, field="V")
→ calls get_screenshot()
← [image showing isosurface]

Agent: Interesting pattern. Let me also add a slice to see the interior.
→ calls create_slice(normal_x=0, normal_y=0, normal_z=1)
→ calls color_by(field="V")
→ calls get_screenshot()
← [image with slice colored by V]

Agent: Let me advance a few steps to see how the pattern evolves.
→ calls advance_step()  (×5)
→ calls get_screenshot()
← [image showing evolved pattern]
```

## Configuration Files

| File | Purpose |
|------|---------|
| `adios2-sst.xml` | ADIOS2 config: SST engine with QueueLimit=3, Discard policy |
| `gs-fides.json` | Fides data model: maps U, V arrays to VTK Cartesian grid |
| `settings-staging.json` | Gray-Scott simulation settings pointing to SST config |
| `streaming_status.json` | Runtime: current step/pause state (auto-generated) |
| `streaming_command.json` | Runtime: MCP→bridge commands (auto-generated) |

## SST Tuning

Edit `adios2-sst.xml` to change the coupling behavior:

- **`QueueLimit`**: How many steps to buffer. Higher = more lag tolerance for the agent.
- **`QueueFullPolicy`**: `Discard` (drop old data, non-blocking sim) or `Block` (sim waits).
- **`RendezvousReaderCount`**: `1` = sim waits for reader; `0` = sim starts immediately.
- **`DataTransport`**: `WAN` for TCP (cross-node), `MPI` for same-node.
