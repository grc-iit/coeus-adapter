"""
ParaView MCP Server

This script runs as a standalone process and:
1. Connects to ParaView using its Python API over network
2. Exposes key ParaView functionality through the MCP protocol
3. Updates visualizations in the existing ParaView viewport

Usage:
1. Start pvserver with --multi-clients flag (e.g., pvserver --multi-clients --server-port=11111)
2. Start ParaView app and connect to the server
3. Configure Claude Desktop to use this script

"""
import os
import sys
import logging
import argparse
import io
from pathlib import Path

# pvpython replaces sys.stdin/stdout/stderr with VTK stream capture objects
# that lack a .buffer attribute. FastMCP's stdio transport reads
# sys.stdin.buffer and writes sys.stdout.buffer for binary JSON-RPC framing,
# so all three must be restored to real fds before importing FastMCP.
_real_stdin = os.fdopen(os.dup(0), "rb", buffering=0)
_real_stdout = os.fdopen(os.dup(1), "wb", buffering=0)
_real_stderr = os.fdopen(os.dup(2), "wb", buffering=0)
sys.stdin = io.TextIOWrapper(_real_stdin, encoding="utf-8")
sys.stdout = io.TextIOWrapper(_real_stdout, encoding="utf-8", write_through=True)
sys.stderr = io.TextIOWrapper(_real_stderr, encoding="utf-8", write_through=True)

from mcp.server.fastmcp import FastMCP, Image
from paraview_manager import ParaViewManager

# Configure logging
log_dir = Path.home() / "paraview_logs"
os.makedirs(log_dir, exist_ok=True)
log_file = log_dir / "paraview_mcp_external.log"

logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s',
    handlers=[
        logging.FileHandler(log_file),
    ]
)

# Default prompt that instructs Claude how to interact with ParaView
default_prompt = """
When using ParaView through this interface, please follow these guidelines:

1. IMPORTANT: Only call strictly necessary ParaView functions per reply (and please limit the total number of call per reply). This ensures operations execute in a more interative manner and no excessive calls to related but non-essential functions. 

2. The only execute multiple repeated function call when given a target goal (e.g., identify a specific object), where different parameters need to used (e.g., isosurface with different isovalue). Avoid repeated calling of color map function unless user specific ask for color map design.

3. Paraview will be connect to mcp server on starup so no need to connect first.


"""
    
logger = logging.getLogger("pv_external_mcp")

# Create the ParaView manager
pv_manager = ParaViewManager()

# Initialize FastMCP server for Claude Desktop integration with default prompt
mcp = FastMCP("ParaView", instructions=default_prompt)

# ============================================================================
# MCP Tools for ParaView
# ============================================================================

@mcp.tool()
def load_data(file_path: str) -> str:
    """
    Load data from a file into ParaView.
    
    Args:
        file_path: Path to the data file (supports VTK, EXODUS, CSV, RAW, etc.)
    
    Returns:
        Status message
    """
    success, message, _, source_name = pv_manager.load_data(file_path)
    if success:
        return f"{message}. Source registered as '{source_name}'."
    else:
        return message

@mcp.tool()
def save_contour_as_stl(stl_filename: str = "contour.stl") -> str:
    """
    Save the currently active contour (or any surface/mesh source) as an STL file
    in the same folder as the originally loaded data.

    Args:
        stl_filename: The STL file name to use, defaults to 'contour.stl'.

    Returns:
        A status message (string).
    """
    success, message, path = pv_manager.save_contour_as_stl(stl_filename)
    return message

@mcp.tool()
def create_source(source_type: str) -> str:
    """
    Create a new geometric source.
    
    Args:
        source_type: Type of source to create (Sphere, Cone, Cylinder, Plane, Box)
    
    Returns:
        Status message
    """
    success, message, _, source_name = pv_manager.create_source(source_type)
    if success:
        return f"{message}. Source registered as '{source_name}'."
    else:
        return message

@mcp.tool()
def create_isosurface(value: float, field: str = None) -> str:
    """
    Create an isosurface visualization of the active source.
    
    Args:
        value: Isovalue
        field: Optional field name to contour by
    
    Returns:
        Status message
    """
    success, message, contour_obj, contour_name = pv_manager.create_isosurface(value, field)
    if success:
        # Return a user-friendly message that also includes the name
        return f"{message}. Filter registered as '{contour_name}'."
    else:
        return message

@mcp.tool()
def create_slice(origin_x: float = None, origin_y: float = None, origin_z: float = None,
                 normal_x: float = 0, normal_y: float = 0, normal_z: float = 1) -> str:
    """
    Create a slice through the loaded volume data.
    
    Args:
        origin_x, origin_y, origin_z: Coordinates for the slice plane's origin. If None,
            defaults to the data set's center.
        normal_x, normal_y, normal_z: Normal vector for the slice plane (default [0, 0, 1]).
    
    Returns:
        A string message containing success/failure details, plus the pipeline name.
    """
    success, message, slice_filter, slice_name = pv_manager.create_slice(
        origin_x,
        origin_y,
        origin_z,
        normal_x,
        normal_y,
        normal_z
    )

    # Return either an error message or a success message including the slice's name
    return message if success else f"Error creating slice: {message}"

@mcp.tool()
def toggle_volume_rendering(enable: bool = True) -> str:
    """
    Toggle the visibility of volume rendering for the active source.
    
    Args:
        enable (bool): Whether to show (True) or hide (False) volume rendering.
                      If True, shows volume rendering (switching to 'Volume' representation if needed).
                      If False, hides the volume but preserves the volume representation settings.
    
    Returns:
        Status message
    """
       
    success, message, source_name = pv_manager.create_volume_rendering(enable)
    if success:
        # Return a user-friendly message that also includes the name
        return f"{message}. Source registered as '{source_name}'."
    else:
        return message

@mcp.tool()
def toggle_visibility(enable: bool = True) -> str:
    """
    Toggle the visibility for the active source.
    
    Args:
        enable (bool): Whether to show (True) or hide (False) the active source.
                      If True, makes the active source visible.
                      If False, hides the active source but preserves the representation settings.
    
    Returns:
        Status message
    """
       
    success, message, source_name = pv_manager.toggle_visibility(enable)
    if success:
        # Return a user-friendly message that also includes the name
        return f"{message}. Source registered as '{source_name}'."
    else:
        return message


@mcp.tool()
def set_active_source(name: str) -> str:
    """
    Set the active pipeline object by its name.

    Usage:
      set_active_source("Contour1")

    Returns a status message.
    """
    success, message = pv_manager.set_active_source(name)
    return message

@mcp.tool()
def get_active_source_names_by_type(source_type: str = None) -> str:
    """
    Get a list of source names filtered by their type.

    Args:
        source_type (str, optional): Filter sources by type (e.g., 'Sphere', 'Contour', etc.).
                                  If None, returns all sources.

    Returns:
        A string message containing the source names or error message.
    """
    success, message, source_names = pv_manager.get_active_source_names_by_type(source_type)
    
    if success and source_names:
        sources_list = "\n- ".join(source_names)
        result = f"{message}:\n- {sources_list}"
        return result
    else:
        return message

# @mcp.tool()
# def edit_volume_opacity(field_name: str, opacity_points: list) -> str:
#     """
#     Edit ONLY the opacity transfer function for the specified field,
#     ensuring we pass only (value, alpha) pairs.

#     [Tips: only needed by volume rendering particularly finetuning the result, likely not needed when the color is ideal, usually the lower value should always have lower opacity]

#     Args:
#         field_name (str): The data array (field) name whose opacity we're adjusting.
#         opacity_points (list of [value, alpha] pairs):
#             Example: [[0.0, 0.0], [50.0, 0.3], [100.0, 1.0]]

#     Returns:
#         A status message (success or error)
#     """
#     success, message = pv_manager.edit_volume_opacity(field_name, opacity_points)
#     return message

# Compatible with OpenAI tool using
@mcp.tool()
def edit_volume_opacity(field_name: str, opacity_points: list[dict[str, float]]) -> str:
    """
    Edit ONLY the opacity transfer function for the specified field.

    Args:
        field_name (str): The scalar field to modify.
        opacity_points (list): A list of dicts like:
            [{"value": 0.0, "alpha": 0.0}, {"value": 50.0, "alpha": 0.3}]

    Returns:
        A status message (success or error)
    """
    formatted_points = [[pt["value"], pt["alpha"]] for pt in opacity_points]
    success, message = pv_manager.edit_volume_opacity(field_name, formatted_points)
    return message

# @mcp.tool()
# def set_color_map(field_name: str, color_points: list) -> str:
#     """
#     Sets the color transfer function for the specified field.

#     [Tips: only volume rendering should be using the set_color_map function, the lower values range corresponds to lower density objects, whereas higher values indicate high physical density. When design the color mapping try to assess the object of interest's density first from the default colormap (low value assigned to blue, high value assigned to red) and re-assign customized color accordingly, the order of the color may need to be adjust based on the rendering result. The more solid object should have higher density (!high value range). And a screen_shot should always be taken once this function is called to assess how to adjust the color_map again.]

#     Args:
#         field_name (str): The name of the field/array (as it appears in ParaView).
#         color_points (list of [value, [r, g, b]]):
#             e.g., [[0.0, [0.0, 0.0, 1.0]], [50.0, [0.0, 1.0, 0.0]], [100.0, [1.0, 0.0, 0.0]]]
#             Each element is (value, (r, g, b)) with r,g,b in [0,1].

#     Returns:
#         A status message as a string (e.g., success or error).
#     """
#     success, message = pv_manager.set_color_map(field_name, color_points)
#     return message

# Compatible with OpenAI tool using
@mcp.tool()
def set_color_map(field_name: str, color_points: list[dict]) -> str:
    """
    Sets the color transfer function for the specified field.

    [Tips: only volume rendering should be using the set_color_map function, the lower values range corresponds to lower density objects, whereas higher values indicate high physical density. When design the color mapping try to assess the object of interest's density first from the default colormap (low value assigned to blue, high value assigned to red) and re-assign customized color accordingly, the order of the color may need to be adjust based on the rendering result. The more solid object should have higher density (!high value range). And a screen_shot should always be taken once this function is called to assess how to adjust the color_map again.]

    Args:
        field_name (str): The name of the field/array (as it appears in ParaView).
        color_points (list of dicts): Each element should be a dict:
            {"value": float, "rgb": [r, g, b]} where r,g,b ∈ [0,1].

            Example:
            [
                {"value": 0.0, "rgb": [0.0, 0.0, 1.0]},
                {"value": 50.0, "rgb": [0.0, 1.0, 0.0]},
                {"value": 100.0, "rgb": [1.0, 0.0, 0.0]}
            ]

    Returns:
        A status message (success or error).
    """
    # Transform color_points to expected internal format: list[tuple[float, tuple[float, float, float]]]
    try:
        formatted_points = [(pt["value"], tuple(pt["rgb"])) for pt in color_points]
    except Exception as e:
        return f"Invalid format for color_points: {e}"

    success, message = pv_manager.set_color_map(field_name, formatted_points)
    return message


@mcp.tool()
def color_by(field: str, component: int = -1) -> str:
    """
    Color the active visualization by a specific field.
    This function first checks if the active source can be colored by fields
    (i.e., it's a dataset with arrays) before attempting to apply colors.
    [tips] Volume rendering should not use this function 

    Args:
        field: Field name to color by
        component: Component to color by (-1 for magnitude)
    
    Returns:
        Status message
    """
    success, message = pv_manager.color_by(field, component)
    return message

@mcp.tool()
def compute_surface_area() -> str:
    """
    Compute the surface area of the currently active dataset.
    NOTE: Must be a surface mesh or 'Area' array won't exist.
    """
    success, message, area_value = pv_manager.compute_surface_area()
    return message

# @mcp.tool()
# def set_color_map_preset(preset_name: str) -> str:
#     """
#     Set the color map (lookup table) for the current visualization.
#     [tips: this should only be call at the beginning of the volume rendering]

#     Args:
#         preset_name: Name of the color map preset (e.g., "Rainbow", "Cool to Warm", "viridis")
    
#     Returns:
#         Status message
#     """
#     success, message = pv_manager.set_color_map(preset_name)
#     return message

@mcp.tool()
def set_representation_type(rep_type: str) -> str:
    """
    Set the representation type for the active source.
    
    [Tips: This function should not be used for volume rendering]

    Args:
        rep_type: Representation type (Surface, Wireframe, Points, etc.)
    
    Returns:
        Status message
    """
    success, message = pv_manager.set_representation_type(rep_type)
    return message

@mcp.tool()
def get_pipeline() -> str:
    """
    Get the current pipeline structure.
    
    Returns:
        Description of the current pipeline
    """
    success, message = pv_manager.get_pipeline()
    return message

@mcp.tool()
def get_available_arrays() -> str:
    """
    Get a list of available arrays in the active source.

    [tips: normally volume rendering would not require this information]

    Returns:
        List of available arrays
    """
    success, message = pv_manager.get_available_arrays()
    return message

@mcp.tool()
def get_histogram(field: str = None, num_bins: int = 32, data_location: str = "POINTS") -> str:
    """
    Compute a histogram of a scalar field on the active source.

    Useful for spotting outliers or unusual value distributions before
    deciding on isovalues or color map ranges.

    Args:
        field: Name of the scalar field. If omitted and the active source
               has exactly one numeric array in the chosen location, that
               array is auto-selected.
        num_bins: Number of histogram bins (default 32).
        data_location: "POINTS" or "CELLS" (default "POINTS").

    Returns:
        Status message followed by tab-separated bin_center / frequency rows.
    """
    success, message, hist_data = pv_manager.get_histogram(field, num_bins, data_location)
    if success and hist_data:
        lines = [message, "", "bin_center\tfrequency"]
        for bin_center, freq in hist_data:
            lines.append(f"{bin_center:.6g}\t{int(freq)}")
        return "\n".join(lines)
    return message

@mcp.tool()
def create_streamline(seed_point_number: int, vector_field: str = None,
                     integration_direction: str = "BOTH", max_steps: int = 1000,
                     initial_step: float = 0.1, maximum_step: float = 50.0) -> str:
    """
    Create streamlines from the loaded vector volume using the StreamTracer filter.
    This function automatically generates seed points based on the data bounds.
    
    Args:
        seed_point_number (int): The number of seed points to automatically generate.
        vector_field (str, optional): The name of the vector field to use for tracing. 
                                    If None, the first vector field will be chosen automatically.
        integration_direction (str): Integration direction ("FORWARD", "BACKWARD", or "BOTH"; default: "BOTH").
        max_steps (int): Maximum number of integration steps (default: 1000).
        initial_step (float): Initial integration step length (default: 0.1).
        maximum_step (float): Maximum streamline length (default: 50.0).
        
    Returns:
        str: Status message indicating whether the streamline was successfully created.
    """
    # Call the stream tracer creation method in your ParaViewManager
    success, message, streamline, tube_name = pv_manager.create_stream_tracer(
        vector_field=vector_field,
        base_source=None,  # Use the active source
        point_center=None,  # Auto-calculate the center
        integration_direction=integration_direction,
        initial_step_length=initial_step,
        maximum_stream_length=maximum_step,
        number_of_streamlines=seed_point_number
    )
    
    if success:
        return f"{message} Tube registered as '{tube_name}'."
    else:
        return message

@mcp.tool()
def get_screenshot():
    """
    Capture a screenshot of the current view and display it in chat.

    Returns:
        Image data or error message
    """
    success, message, img_path = pv_manager.get_screenshot()

    if not success:
        return message
    else:
        return Image(path=img_path)
    
@mcp.tool()
def rotate_camera(azimuth: float = 30.0, elevation: float = 0.0) -> str:
    """
    Rotate the camera by specified angles.
    
    Args:
        azimuth: Rotation around vertical axis in degrees
        elevation: Rotation around horizontal axis in degrees
    
    Returns:
        Status message
    """
    success, message = pv_manager.rotate_camera(azimuth, elevation)
    return message

@mcp.tool()
def reset_camera() -> str:
    """
    Reset the camera to show all data.
    
    Returns:
        Status message
    """
    success, message = pv_manager.reset_camera()
    return message

# @mcp.tool()
# def plot_over_line(point1: list = None, point2: list = None, resolution: int = 100) -> str:
#     """
#     Create a 'Plot Over Line' filter to sample data along a line between two points.

#     Args:
#         point1 (list, optional): The [x, y, z] coordinates of the start point. If None, will use data bounds.
#         point2 (list, optional): The [x, y, z] coordinates of the end point. If None, will use data bounds.
#         resolution (int, optional): Number of sample points along the line (default: 100).

#     Returns:
#         Status message
#     """
#     success, message, plot_filter = pv_manager.plot_over_line(point1, point2, resolution)
#     return message

# Compatible with OpenAI tool using
@mcp.tool()
def plot_over_line(point1: list[float] = None, point2: list[float] = None, resolution: int = 100) -> str:
    """
    Sample the active scalar field along a line between two points and return
    the actual sampled values, plus a summary highlighting the maximum value
    and its location along the line. Use this to numerically locate features
    (e.g., the peak of a Gaussian hot spot) without screenshots.

    Args:
        point1: [x, y, z] start point. If omitted, defaults to the data min corner.
        point2: [x, y, z] end point.   If omitted, defaults to the data max corner.
        resolution: Number of samples along the line (default 100).

    Returns:
        A multi-line string: a header summarizing the line and the location of
        the maximum, followed by tab-separated rows of t<TAB>value<TAB>x<TAB>y<TAB>z.
    """
    success, message, samples = pv_manager.plot_over_line(point1, point2, resolution)
    if not success or not samples:
        return message

    # Pick the most plausible scalar column (skip helpers and coords).
    skip = {"arc_length", "vtkValidPointMask",
            "Points_0", "Points_1", "Points_2",
            "_x", "_y", "_z"}
    scalar_keys = [k for k in samples[0].keys() if k not in skip]
    if not scalar_keys:
        return f"{message}\n(no scalar columns found)"
    field = scalar_keys[0]

    n = len(samples)
    max_idx = max(range(n), key=lambda i: samples[i].get(field, float("-inf")))
    max_row = samples[max_idx]
    max_val = max_row.get(field)
    max_xyz = (max_row.get("_x"), max_row.get("_y"), max_row.get("_z"))

    lines = [
        message,
        f"scalar column: {field}",
        f"max value:     {max_val:.6g}  at sample {max_idx}/{n-1}"
        f"  xyz=({max_xyz[0]:.2f}, {max_xyz[1]:.2f}, {max_xyz[2]:.2f})",
        "",
        "i\tt\tvalue\tx\ty\tz",
    ]
    for i, row in enumerate(samples):
        t = row.get("arc_length", float(i))
        v = row.get(field, float("nan"))
        x, y, z = row.get("_x", 0.0), row.get("_y", 0.0), row.get("_z", 0.0)
        lines.append(f"{i}\t{t:.4g}\t{v:.6g}\t{x:.3f}\t{y:.3f}\t{z:.3f}")
    return "\n".join(lines)

@mcp.tool()
def find_connected_components(threshold: float, field: str = None,
                              location: str = "POINTS") -> str:
    """
    Threshold the active scalar field above `threshold`, then find every
    connected component above that threshold and return one entry per
    component with its centroid, bounding box, and point count.

    Use this when the question is "how many distinct objects are in this
    volume?" or "where are the blobs?". It is dramatically more efficient
    than line probing for enumerating multiple peaks at unknown locations.

    Args:
        threshold (float): Lower bound; voxels with value >= threshold are
            considered "in" an object.
        field (str): Scalar field name. If omitted and there is only one
            available array, that array is auto-selected.
        location (str): "POINTS" (default) or "CELLS".

    Returns:
        A multi-line string: a header with the component count, followed by
        one tab-separated row per component:
            id<TAB>n_points<TAB>cx<TAB>cy<TAB>cz<TAB>x0<TAB>x1<TAB>y0<TAB>y1<TAB>z0<TAB>z1
    """
    success, message, regions = pv_manager.find_connected_components(
        threshold, field, location
    )
    if not success or regions is None:
        return message
    if not regions:
        return f"{message} (no components found — try a lower threshold)"

    lines = [
        message,
        "id\tn_points\tcx\tcy\tcz\tx0\tx1\ty0\ty1\tz0\tz1",
    ]
    for r in regions:
        cx, cy, cz = r["centroid"]
        x0, x1, y0, y1, z0, z1 = r["bounds"]
        lines.append(
            f"{r['region_id']}\t{r['n_points']}\t"
            f"{cx:.3f}\t{cy:.3f}\t{cz:.3f}\t"
            f"{x0:.2f}\t{x1:.2f}\t{y0:.2f}\t{y1:.2f}\t{z0:.2f}\t{z1:.2f}"
        )
    return "\n".join(lines)

@mcp.tool()
def warp_by_vector(vector_field: str = None, scale_factor: float = 1.0) -> str:
    """
    Apply the 'Warp By Vector' filter to the active source.

    Args:
        vector_field (str, optional): The name of the vector field to use for warping. If None, the first available vector field will be used.
        scale_factor (float, optional): The scale factor for the warp (default: 1.0).

    Returns:
        Status message
    """
    success, message, warp_filter = pv_manager.warp_by_vector(vector_field, scale_factor)
    return message

@mcp.tool()
def list_commands() -> str:
    """
    List all available commands in this ParaView MCP server.
    
    Returns:
        List of available commands
    """
    commands = [
        "load_data: Load data from a file",
        "create_source: Create a geometric source (Sphere, Cone, etc.)",
        "create_isosurface: Create an isosurface visualization",
        "create_slice: Create a slice through the data",
        "toggle_volume_rendering: Enable or disable volume rendering",
	    "toggle_visibility: Enable or disable visibility for the active source",
        "set_active_source: Set the active pipeline object by name",
        "get_active_source_names_by_type: Get a list of sources filtered by type",
        "color_by: Color the visualization by a field",
        # "set_color_map_preset: Set the color map preset",
        "set_representation_type: Set the representation type (Surface, Wireframe, etc.)",
        "edit_volume_opacity: Edit the opacity transfer function",
        "get_pipeline: Get the current pipeline structure",
        "get_available_arrays: Get available data arrays",
        "get_histogram: Compute a histogram of a scalar field on the active source",
        "find_connected_components: Find all connected components above a threshold (returns centroids + bounds)",
        "create_streamline: Create stream line visualization",
        "compute_surface_area: Compute the surface area of the active surface",
        "save_contour_as_stl: Save the active surface as STL",
        "get_screenshot: Capture a screenshot and display it in chat",
        "rotate_camera: Rotate the camera view",
        "reset_camera: Reset the camera to show all data",
        "plot_line: Plot a line through the data",
        "warp_by_vector: Warp the active source by a vector field",
    ]
    
    return "Available ParaView commands:\n\n" + "\n".join(commands)


def main():
    parser = argparse.ArgumentParser(description="ParaView External MCP Server")
    parser.add_argument("--server", type=str, default="localhost", help="ParaView server hostname (default: localhost)")
    parser.add_argument("--port", type=int, default=11111, help="ParaView server port (default: 11111)")
    parser.add_argument("--paraview_package_path", type=str, help="Path to the ParaView Python package", default=None)
    
    args = parser.parse_args()

    # Add the ParaView package path to sys.path
    if args.paraview_package_path:
        sys.path.append(args.paraview_package_path)
    
    # Connect to ParaView
    pv_manager.connect(args.server, args.port)
    
    # Run the MCP server
    try:
        logger.info("Starting ParaView External MCP Server")
        logger.info(f"ParaView server: {args.server}:{args.port}")
        # logger.info("Default prompt enabled: Claude will call one function per reply")
        
        # Run the MCP server
        mcp.run()
    except KeyboardInterrupt:
        logger.info("Server stopped by user")
    except Exception as e:
        logger.error(f"Error running MCP server: {str(e)}")

if __name__ == "__main__":
    main()
