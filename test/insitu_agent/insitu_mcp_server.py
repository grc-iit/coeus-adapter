"""
In-Situ ParaView MCP Server

Extends the ParaView MCP server with streaming-aware tools so an AI agent
can interactively explore live simulation data from an ADIOS2 SST stream.

The streaming bridge (insitu_streaming.py) reads SST data into a pvserver.
This MCP server connects to the same pvserver and provides:
  - All standard ParaView MCP tools (isosurface, slice, screenshot, etc.)
  - Streaming control tools (pause, resume, advance step, get status)

Usage:
  python insitu_mcp_server.py --server localhost --port 11111 \
      --status-file streaming_status.json
"""

import os
import sys
import io
import json
import logging
import argparse
from pathlib import Path

# pvpython replaces sys.stdout/stdin with VTK wrappers that lack .buffer,
# breaking MCP's stdio transport. Restore real file descriptors before
# importing MCP.
if not hasattr(sys.stdout, 'buffer'):
    sys.stdout = io.TextIOWrapper(io.FileIO(1, 'wb', closefd=False), write_through=True)
if not hasattr(sys.stdin, 'buffer'):
    sys.stdin = io.TextIOWrapper(io.FileIO(0, 'rb', closefd=False))

from mcp.server.fastmcp import FastMCP, Image

# Ares cluster MPI environment — needed when MCP server runs under pvpython
os.environ['OMPI_MCA_pml'] = 'ob1'
os.environ['OMPI_MCA_btl'] = 'tcp,self'
os.environ['OMPI_MCA_osc'] = '^ucx'
os.environ['OMPI_MCA_btl_tcp_if_include'] = 'eno1'
os.environ['OMPI_MCA_oob_tcp_if_include'] = 'eno1'

# Add the paraview_mcp directory to the path so we can import ParaViewManager
SCRIPT_DIR = Path(__file__).resolve().parent
PARAVIEW_MCP_DIR = Path.home() / "software" / "paraview_mcp"
sys.path.insert(0, str(PARAVIEW_MCP_DIR))

from paraview_manager import ParaViewManager

log_dir = Path.home() / "paraview_logs"
os.makedirs(log_dir, exist_ok=True)
log_file = log_dir / "insitu_mcp.log"

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s - %(name)s - %(levelname)s - %(message)s",
    handlers=[
        logging.FileHandler(log_file),
        logging.StreamHandler(),
    ],
)

logger = logging.getLogger("insitu_mcp")

INSITU_PROMPT = """
You are an AI agent controlling a live in-situ scientific visualization pipeline.
A Gray-Scott reaction-diffusion simulation is streaming data via ADIOS2 SST,
and you can interact with it in real-time through ParaView.

Key capabilities:
1. **Streaming control**: pause/resume the stream, advance one step at a time,
   or check what timestep you're on with get_streaming_status.
2. **Visualization**: create isosurfaces, slices, volume renderings, etc.
   on the LIVE simulation data — these update as new steps arrive.
3. **Inspection**: take screenshots, query available arrays, check data ranges.

Recommended workflow:
- Start by calling get_streaming_status to see the current state.
- Pause the stream if you want to carefully explore a single timestep.
- Use get_available_arrays to discover what fields are available (U, V).
- Apply filters (isosurface, slice) to explore the data.
- Take screenshots to observe results and iterate.
- Resume or advance_step to move through the simulation.

The simulation has two scalar fields:
- U: reactant concentration
- V: product concentration (typically more interesting for visualization)

IMPORTANT: Only call strictly necessary ParaView functions per reply.
Pause the stream before doing multi-step explorations on a single timestep.
"""

pv_manager = ParaViewManager()
mcp = FastMCP("InSitu-ParaView", instructions=INSITU_PROMPT)

# Lazy connection state — pvserver may not be running when MCP starts
_pv_connected = False
_pv_server = "localhost"
_pv_port = 11112


def _ensure_connected():
    """Lazily connect to pvserver on first tool call."""
    global _pv_connected
    if not _pv_connected:
        logger.info(f"Lazy-connecting to pvserver at {_pv_server}:{_pv_port}")
        _pv_connected = pv_manager.connect(_pv_server, _pv_port)
        if not _pv_connected:
            logger.warning("Failed to connect to pvserver — visualization tools will fail")
    return _pv_connected

STATUS_FILE_PATH = None
TIMING_FILE = None
SCREENSHOT_FILE = None


def timed_tool(func):
    """Decorator: measures total MCP tool time and PV operation time."""
    import functools
    import time as _time

    @functools.wraps(func)
    def wrapper(*args, **kwargs):
        t0 = _time.monotonic()
        result = func(*args, **kwargs)
        mcp_total_ms = (_time.monotonic() - t0) * 1000
        pv_op_ms = getattr(wrapper, '_last_pv_ms', 0)
        if TIMING_FILE:
            try:
                with open(TIMING_FILE, "a") as f:
                    json.dump({
                        "tool": func.__name__,
                        "pv_operation_ms": round(pv_op_ms, 2),
                        "mcp_total_ms": round(mcp_total_ms, 2),
                        "mcp_overhead_ms": round(mcp_total_ms - pv_op_ms, 2),
                        "timestamp": _time.time(),
                    }, f)
                    f.write("\n")
            except OSError:
                pass
        return result
    return wrapper


def _timed_pv(tool_wrapper, pv_method, *args, **kwargs):
    """Time a ParaViewManager method call and store on the tool wrapper."""
    import time as _time
    t0 = _time.monotonic()
    result = pv_method(*args, **kwargs)
    tool_wrapper._last_pv_ms = (_time.monotonic() - t0) * 1000
    return result


def _read_streaming_status():
    """Read the streaming status JSON written by insitu_streaming.py."""
    if STATUS_FILE_PATH and os.path.exists(STATUS_FILE_PATH):
        try:
            with open(STATUS_FILE_PATH, "r") as f:
                return json.load(f)
        except (json.JSONDecodeError, OSError):
            return None
    return None


def _write_streaming_command(command):
    """
    Write a command to the streaming bridge via a command file.
    The streaming bridge polls this file to receive pause/resume/advance commands.
    """
    cmd_file = STATUS_FILE_PATH.replace("streaming_status.json", "streaming_command.json") if STATUS_FILE_PATH else "streaming_command.json"
    try:
        with open(cmd_file, "w") as f:
            json.dump(command, f)
        return True
    except OSError as e:
        logger.error(f"Failed to write command: {e}")
        return False


# ============================================================================
# Streaming Control Tools
# ============================================================================

@mcp.tool()
@timed_tool
def get_streaming_status() -> str:
    """
    Get the current status of the in-situ streaming pipeline.

    Returns:
        Current timestep, whether the stream is paused, and whether it has ended.
    """
    status = _read_streaming_status()
    if status is None:
        return (
            "Streaming status unavailable. The streaming bridge may not be running. "
            "Make sure insitu_streaming.py is running and connected to the same pvserver."
        )

    parts = [
        f"Current timestep: {status.get('step', 'unknown')}",
        f"Paused: {status.get('paused', 'unknown')}",
        f"Stream ended: {status.get('ended', 'unknown')}",
        f"Pipeline ready: {status.get('pipeline_ready', 'unknown')}",
    ]
    return "Streaming status:\n" + "\n".join(parts)


@mcp.tool()
@timed_tool
def pause_streaming() -> str:
    """
    Pause the streaming pipeline. Data stays at the current timestep,
    allowing you to interactively explore it with visualization tools.

    Returns:
        Status message
    """
    success = _write_streaming_command({"action": "pause"})
    if success:
        return "Pause command sent. The stream will hold at the current timestep."
    return "Failed to send pause command."


@mcp.tool()
@timed_tool
def resume_streaming() -> str:
    """
    Resume the streaming pipeline. New timesteps will be read automatically.

    Returns:
        Status message
    """
    success = _write_streaming_command({"action": "resume"})
    if success:
        return "Resume command sent. The stream will continue advancing."
    return "Failed to send resume command."


@mcp.tool()
@timed_tool
def advance_step() -> str:
    """
    Advance the stream by exactly one timestep, then pause again.
    Useful for stepping through the simulation frame by frame.

    Returns:
        Status message
    """
    success = _write_streaming_command({"action": "advance_one"})
    if success:
        return "Advance command sent. The stream will read one step and pause."
    return "Failed to send advance command."


# ============================================================================
# Standard ParaView MCP Tools (from paraview_mcp_server.py)
# ============================================================================

@mcp.tool()
@timed_tool
def get_screenshot():
    """
    Capture a screenshot of the current view and display it in chat.

    Returns:
        Image data or error message
    """
    # Prefer reading the bridge-saved screenshot file. The bridge's own
    # process renders its view (with the slice visualization attached)
    # after every SST step and atomically writes a PNG to SCREENSHOT_FILE.
    # Reading that file gives us the true pixels the bridge produced,
    # without relying on MCP's local ParaView client state — which does
    # not know about the bridge's Show(slice, ...) call in another process.
    if SCREENSHOT_FILE and os.path.exists(SCREENSHOT_FILE):
        import time as _time
        t0 = _time.monotonic()
        try:
            with open(SCREENSHOT_FILE, "rb") as f:
                data = f.read()
            get_screenshot._last_pv_ms = (_time.monotonic() - t0) * 1000
            # Write to a unique temp file so FastMCP can re-read it
            import tempfile
            with tempfile.NamedTemporaryFile(suffix=".png", delete=False) as tmp:
                tmp.write(data)
                tmp_path = tmp.name
            return Image(path=tmp_path)
        except Exception as e:
            logger.warning(f"Failed to read bridge screenshot {SCREENSHOT_FILE}: {e}")

    # Fallback: use pv_manager (historical path, often returns empty pixels
    # because MCP's local view state doesn't match the bridge's).
    if not _ensure_connected():
        return "Not connected to pvserver"
    success, message, img_path = _timed_pv(get_screenshot, pv_manager.get_screenshot)
    if not success:
        return message
    return Image(path=img_path)


@mcp.tool()
@timed_tool
def create_isosurface(value: float, field: str = None) -> str:
    """
    Create an isosurface visualization on the live simulation data.

    Args:
        value: Isovalue
        field: Field name to contour by (e.g. "U" or "V")

    Returns:
        Status message
    """
    if not _ensure_connected():
        return "Not connected to pvserver"
    success, message, _, contour_name = _timed_pv(create_isosurface, pv_manager.create_isosurface, value, field)
    if success:
        return f"{message}. Filter registered as '{contour_name}'."
    return message


@mcp.tool()
@timed_tool
def create_slice(
    origin_x: float = None, origin_y: float = None, origin_z: float = None,
    normal_x: float = 0, normal_y: float = 0, normal_z: float = 1,
) -> str:
    """
    Create a slice through the live simulation volume.

    Args:
        origin_x, origin_y, origin_z: Slice plane origin. Defaults to data center.
        normal_x, normal_y, normal_z: Slice plane normal (default [0,0,1]).

    Returns:
        Status message
    """
    if not _ensure_connected():
        return "Not connected to pvserver"
    success, message, _, slice_name = _timed_pv(create_slice, pv_manager.create_slice,
        origin_x, origin_y, origin_z, normal_x, normal_y, normal_z
    )
    return message if success else f"Error creating slice: {message}"


@mcp.tool()
@timed_tool
def toggle_volume_rendering(enable: bool = True) -> str:
    """
    Toggle volume rendering for the simulation data.

    Args:
        enable: True to show volume rendering, False to hide it.

    Returns:
        Status message
    """
    if not _ensure_connected():
        return "Not connected to pvserver"
    success, message, source_name = pv_manager.create_volume_rendering(enable)
    if success:
        return f"{message}. Source: '{source_name}'."
    return message


@mcp.tool()
@timed_tool
def toggle_visibility(enable: bool = True) -> str:
    """
    Toggle visibility for the active source.

    Args:
        enable: True to show, False to hide.

    Returns:
        Status message
    """
    if not _ensure_connected():
        return "Not connected to pvserver"
    success, message, source_name = pv_manager.toggle_visibility(enable)
    if success:
        return f"{message}. Source: '{source_name}'."
    return message


@mcp.tool()
@timed_tool
def set_active_source(name: str) -> str:
    """
    Set the active pipeline object by its name.

    Args:
        name: Pipeline object name (e.g. "Contour1", "Slice1")

    Returns:
        Status message
    """
    if not _ensure_connected():
        return "Not connected to pvserver"
    success, message = pv_manager.set_active_source(name)
    return message


@mcp.tool()
@timed_tool
def get_active_source_names_by_type(source_type: str = None) -> str:
    """
    Get a list of source names filtered by type.

    Args:
        source_type: Filter by type (e.g. "Contour", "Slice"). None for all.

    Returns:
        List of source names
    """
    if not _ensure_connected():
        return "Not connected to pvserver"
    success, message, source_names = pv_manager.get_active_source_names_by_type(source_type)
    if success and source_names:
        return f"{message}:\n- " + "\n- ".join(source_names)
    return message


@mcp.tool()
@timed_tool
def color_by(field: str, component: int = -1) -> str:
    """
    Color the active visualization by a specific field.

    Args:
        field: Field name (e.g. "U", "V")
        component: Component index (-1 for magnitude)

    Returns:
        Status message
    """
    if not _ensure_connected():
        return "Not connected to pvserver"
    success, message = pv_manager.color_by(field, component)
    return message


@mcp.tool()
@timed_tool
def set_color_map(field_name: str, color_points: list[dict]) -> str:
    """
    Set the color transfer function for a field.

    Args:
        field_name: Scalar field name
        color_points: List of dicts: [{"value": float, "rgb": [r, g, b]}]

    Returns:
        Status message
    """
    try:
        formatted = [(pt["value"], tuple(pt["rgb"])) for pt in color_points]
    except Exception as e:
        return f"Invalid format for color_points: {e}"
    if not _ensure_connected():
        return "Not connected to pvserver"
    success, message = pv_manager.set_color_map(field_name, formatted)
    return message


@mcp.tool()
@timed_tool
def edit_volume_opacity(field_name: str, opacity_points: list[dict[str, float]]) -> str:
    """
    Edit the opacity transfer function for a field.

    Args:
        field_name: Scalar field name
        opacity_points: List of dicts: [{"value": float, "alpha": float}]

    Returns:
        Status message
    """
    if not _ensure_connected():
        return "Not connected to pvserver"
    formatted = [[pt["value"], pt["alpha"]] for pt in opacity_points]
    success, message = pv_manager.edit_volume_opacity(field_name, formatted)
    return message


@mcp.tool()
@timed_tool
def set_representation_type(rep_type: str) -> str:
    """
    Set the representation type for the active source.

    Args:
        rep_type: "Surface", "Wireframe", "Points", "Volume", etc.

    Returns:
        Status message
    """
    if not _ensure_connected():
        return "Not connected to pvserver"
    success, message = pv_manager.set_representation_type(rep_type)
    return message


@mcp.tool()
@timed_tool
def get_pipeline() -> str:
    """
    Get the current pipeline structure showing all sources and filters.

    Returns:
        Pipeline description
    """
    if not _ensure_connected():
        return "Not connected to pvserver"
    success, message = pv_manager.get_pipeline()
    return message


@mcp.tool()
@timed_tool
def get_available_arrays() -> str:
    """
    Get available data arrays (fields) in the active source.

    Returns:
        List of point and cell data arrays
    """
    if not _ensure_connected():
        return "Not connected to pvserver"
    success, message = pv_manager.get_available_arrays()
    return message


@mcp.tool()
@timed_tool
def compute_surface_area() -> str:
    """
    Compute the surface area of the active surface mesh.

    Returns:
        Surface area value
    """
    if not _ensure_connected():
        return "Not connected to pvserver"
    success, message, _ = pv_manager.compute_surface_area()
    return message


@mcp.tool()
@timed_tool
def save_contour_as_stl(stl_filename: str = "contour.stl") -> str:
    """
    Save the active contour/surface as an STL file.

    Args:
        stl_filename: Output filename

    Returns:
        Status message
    """
    if not _ensure_connected():
        return "Not connected to pvserver"
    success, message, _ = pv_manager.save_contour_as_stl(stl_filename)
    return message


@mcp.tool()
@timed_tool
def rotate_camera(azimuth: float = 30.0, elevation: float = 0.0) -> str:
    """
    Rotate the camera by specified angles.

    Args:
        azimuth: Rotation around vertical axis (degrees)
        elevation: Rotation around horizontal axis (degrees)

    Returns:
        Status message
    """
    if not _ensure_connected():
        return "Not connected to pvserver"
    success, message = pv_manager.rotate_camera(azimuth, elevation)
    return message


@mcp.tool()
@timed_tool
def reset_camera() -> str:
    """
    Reset the camera to show all data.

    Returns:
        Status message
    """
    if not _ensure_connected():
        return "Not connected to pvserver"
    success, message = pv_manager.reset_camera()
    return message


@mcp.tool()
@timed_tool
def plot_over_line(
    point1: list[float] = None, point2: list[float] = None, resolution: int = 100
) -> str:
    """
    Sample data along a line between two points.

    Args:
        point1: Start point [x, y, z]. None for data bounds.
        point2: End point [x, y, z]. None for data bounds.
        resolution: Number of sample points (default: 100)

    Returns:
        Status message
    """
    if not _ensure_connected():
        return "Not connected to pvserver"
    success, message, _ = pv_manager.plot_over_line(point1, point2, resolution)
    return message


@mcp.tool()
@timed_tool
def create_streamline(
    seed_point_number: int,
    vector_field: str = None,
    integration_direction: str = "BOTH",
    max_steps: int = 1000,
    initial_step: float = 0.1,
    maximum_step: float = 50.0,
) -> str:
    """
    Create streamlines from the active vector volume.

    Args:
        seed_point_number: Number of seed points
        vector_field: Vector field name (auto-detected if None)
        integration_direction: "FORWARD", "BACKWARD", or "BOTH"
        max_steps: Max integration steps
        initial_step: Initial step length
        maximum_step: Maximum streamline length

    Returns:
        Status message
    """
    if not _ensure_connected():
        return "Not connected to pvserver"
    success, message, _, tube_name = pv_manager.create_stream_tracer(
        vector_field=vector_field,
        base_source=None,
        point_center=None,
        integration_direction=integration_direction,
        initial_step_length=initial_step,
        maximum_stream_length=maximum_step,
        number_of_streamlines=seed_point_number,
    )
    if success:
        return f"{message} Tube registered as '{tube_name}'."
    return message


@mcp.tool()
@timed_tool
def list_commands() -> str:
    """
    List all available commands in this in-situ ParaView MCP server.

    Returns:
        List of available commands
    """
    commands = [
        "--- Streaming Control ---",
        "get_streaming_status: Check current timestep and stream state",
        "pause_streaming: Pause the stream to explore current data",
        "resume_streaming: Resume auto-advancing through timesteps",
        "advance_step: Advance exactly one timestep then pause",
        "",
        "--- Visualization ---",
        "create_isosurface: Create an isosurface on live data",
        "create_slice: Create a slice plane through the volume",
        "toggle_volume_rendering: Enable/disable volume rendering",
        "toggle_visibility: Show/hide the active source",
        "color_by: Color by a specific field",
        "set_color_map: Set custom color transfer function",
        "edit_volume_opacity: Edit opacity transfer function",
        "set_representation_type: Change representation (Surface, Wireframe, etc.)",
        "create_streamline: Create streamline visualization",
        "",
        "--- Inspection ---",
        "get_screenshot: Capture current view as image",
        "get_pipeline: Show the current pipeline structure",
        "get_available_arrays: List available data arrays",
        "compute_surface_area: Compute surface area of active mesh",
        "set_active_source: Set active pipeline object by name",
        "get_active_source_names_by_type: List sources by type",
        "",
        "--- Camera ---",
        "rotate_camera: Rotate the camera view",
        "reset_camera: Reset camera to show all data",
        "",
        "--- Export ---",
        "save_contour_as_stl: Save active surface as STL",
        "plot_over_line: Sample data along a line",
    ]
    return "Available in-situ ParaView commands:\n\n" + "\n".join(commands)


def main():
    global STATUS_FILE_PATH

    parser = argparse.ArgumentParser(description="In-Situ ParaView MCP Server")
    parser.add_argument(
        "--server", type=str, default="localhost",
        help="ParaView server hostname (default: localhost)",
    )
    parser.add_argument(
        "--port", type=int, default=11112,
        help="ParaView server port (default: 11112)",
    )
    parser.add_argument(
        "--status-file", type=str, default="streaming_status.json",
        help="Path to the streaming status JSON file written by insitu_streaming.py",
    )
    parser.add_argument(
        "--paraview_package_path", type=str, default=None,
        help="Path to the ParaView Python package",
    )
    parser.add_argument(
        "--timing-file", type=str, default=None,
        help="Path to write per-tool timing JSONL file",
    )
    parser.add_argument(
        "--screenshot-file", type=str, default=None,
        help="Path to the bridge-saved screenshot PNG. When set, get_screenshot "
             "reads this file instead of asking pvserver to re-render from the "
             "MCP client's local state.",
    )

    args = parser.parse_args()

    if args.paraview_package_path:
        sys.path.append(args.paraview_package_path)

    STATUS_FILE_PATH = os.path.abspath(args.status_file)

    # Store connection params for lazy connect (pvserver may not be up yet)
    global _pv_server, _pv_port, TIMING_FILE, SCREENSHOT_FILE
    _pv_server = args.server
    _pv_port = args.port
    TIMING_FILE = args.timing_file
    SCREENSHOT_FILE = os.path.abspath(args.screenshot_file) if args.screenshot_file else None

    try:
        logger.info("Starting In-Situ ParaView MCP Server")
        logger.info(f"ParaView server: {args.server}:{args.port} (lazy connect)")
        logger.info(f"Status file: {STATUS_FILE_PATH}")
        mcp.run()
    except KeyboardInterrupt:
        logger.info("Server stopped by user")
    except Exception as e:
        logger.error(f"Error running MCP server: {e}")


if __name__ == "__main__":
    main()
