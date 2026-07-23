"""
ParaView MCP Server

This class encapsulates paraview.simple API to expose a higher-level API that is compatible with LLM access/control.
"""

import logging
import re
from paraview.simple import *

class ParaViewManager:
    """
    Encapsulates all ParaView-specific functionality.
    This class handles the interaction with ParaView and provides
    a clean interface for the MCP server.
    """

    def __init__(self):
        """Initialize the ParaView manager"""
        self.connection = None
        self.logger = logging.getLogger("paraview_manager")
        # This will always hold the originally loaded data source,
        # which is needed for operations like volume rendering.
        self.original_source = None
        self._data_folder = ""
    
    def _get_source_name(self, proxy):
        """
        Get the name (registered name) of a source proxy.
        
        Args:
            proxy: The source proxy object.
            
        Returns:
            str: The name of the proxy, or empty string if not found.
        """
        try:
            from paraview.simple import GetSources
            
            if proxy is None:
                return ""
                
            sources_dict = GetSources()
            for (key, src_proxy) in sources_dict.items():
                if src_proxy == proxy:
                    return key[0]  # Return the first element (name) of the key tuple
            
            return ""  # Return empty string if proxy not found
        except Exception as e:
            self.logger.error(f"Error getting source name: {str(e)}")
            return ""
    
    def connect(self, server_url="localhost", port=11112):
        """
        Connect to a running ParaView server

        Args:
            server_url: Server hostname (default: localhost)
            port: Server port (default: 11112)
            
        Returns:
            bool: Success status
        """
        try:
            # Import the paraview.simple module
            import importlib.util
            
            if importlib.util.find_spec("paraview.simple") is not None:
                from paraview.simple import Connect, GetActiveView
                
                # Connect to the existing ParaView server
                full_server_url = f"{server_url}:{port}" if port else server_url
                self.logger.info(f"Connecting to ParaView at {full_server_url}")
                self.connection = Connect(full_server_url)
                
                # Get the active view to confirm connection
                view = GetActiveView()
                
                self.logger.info("Successfully connected to ParaView")
                return True
            else:
                self.logger.warning("paraview.simple module not found. Running in simulation mode.")
                return False
        except Exception as e:
            self.logger.error(f"Failed to connect to ParaView: {str(e)}")
            return False
    
    def load_data(self, file_path):
        """
        Load data from a file into ParaView
        
        Args:
            file_path: Path to the data file
            
        Returns:
            tuple: (success, message, reader, source_name)
        """
        try:
            import os
            from paraview.simple import OpenDataFile, Show, GetActiveView
            
            # Record the directory of the loaded file so we can re-use it.
            self._data_folder = os.path.dirname(file_path)

            # Get file extension
            _, file_extension = os.path.splitext(file_path)
            file_extension = file_extension.lower()
            file_name = os.path.basename(file_path)

            # Special handling for raw volume files
            if file_extension == '.raw':
                reader = self._configure_raw_reader(file_path, file_name)
            else:
                # Standard loading for other file types
                reader = OpenDataFile(file_path)
            
            if not reader:
                return False, f"Failed to load data from {file_path}", None, ""
            
            # Show: this creates an active view if one doesn't already exist
            # (e.g., when connected headlessly to pvserver with no GUI client).
            display = Show(reader)
            display.ScaleFactor = 0.5
            view = GetActiveView()
            if view is not None:
                view.ResetCamera(False)
            # Save the loaded reader as the original data source
            self.original_source = reader
            
            # Get the source name using the helper function
            source_name = self._get_source_name(reader)
            
            return True, f"Successfully loaded data from {file_path}", reader, source_name
        except Exception as e:
            self.logger.error(f"Error loading data: {str(e)}")
            return False, f"Error loading data: {str(e)}", None, ""

    
    def _configure_raw_reader(self, file_path, file_name):
        """
        Configure a reader for RAW volume files
        
        Args:
            file_path: Path to the RAW file
            file_name: Name of the file
            
        Returns:
            reader: Configured reader object
        """
        from paraview.simple import OpenDataFile
        
        # Try to parse dimensions and data type from filename
        # Expected format: name_XxYxZ_datatype.raw (e.g., foot_256x256x256_uint8.raw)
        dimensions_match = re.search(r'(\d+)x(\d+)x(\d+)', file_name)
        datatype_match = re.search(r'_(uint8|uint16|int8|int16|float32|float64)', file_name.lower())
        
        # Load the raw file
        reader = OpenDataFile(file_path)
        if not reader:
            return None
        
        # Set reader properties based on filename
        if dimensions_match:
            dim_x = int(dimensions_match.group(1))
            dim_y = int(dimensions_match.group(2))
            dim_z = int(dimensions_match.group(3))
            reader.DataExtent = [0, dim_x-1, 0, dim_y-1, 0, dim_z-1]
            reader.FileDimensionality = 3
            self.logger.info(f"Detected dimensions: {dim_x}x{dim_y}x{dim_z}")
        
        if datatype_match:
            datatype = datatype_match.group(1)
            # Map to ParaView data types
            datatype_map = {
                'uint8': 'unsigned char',
                'uint16': 'unsigned short',
                'int8': 'char',
                'int16': 'short',
                'float32': 'float',
                'float64': 'double'
            }
            if datatype in datatype_map:
                reader.DataScalarType = datatype_map[datatype]
                self.logger.info(f"Detected data type: {datatype_map[datatype]}")
        else:
            # Default to unsigned char if not specified
            reader.DataScalarType = 'unsigned char'
        
        # Set other common properties for raw files
        reader.DataByteOrder = 'LittleEndian'  # Default to LittleEndian
        reader.NumberOfScalarComponents = 1    # Default to single component
        
        self.logger.info(f"Configured RAW reader with: ScalarType={reader.DataScalarType}, " +
                         f"ByteOrder={reader.DataByteOrder}, Extent={reader.DataExtent}")
        
        return reader
    
    def save_contour_as_stl(self, stl_filename="contour.stl"):
        """
        Save the active source (e.g. a contour) as an STL file in the same folder
        where the original data was loaded.

        Args:
            stl_filename (str): Name of the STL file to create (defaults to 'contour.stl').

        Returns:
            tuple: (success: bool, message: str, saved_path: str)
        """
        try:
            import os
            from paraview.simple import GetActiveSource, SaveData

            # Ensure we have an active source
            active_source = GetActiveSource()
            if not active_source:
                return False, "Error: No active source to save.", ""

            # Check that we have a recorded data folder
            if not hasattr(self, "_data_folder") or not self._data_folder:
                return False, (
                    "Error: No data folder known. "
                    "Did you load data first before saving?"
                ), ""

            # Compose the full path in the same folder as the loaded data
            full_path = os.path.join(self._data_folder, stl_filename)

            # Save to STL
            SaveData(full_path, proxy=active_source)
            
            message = f"Saved active source to STL at: {full_path}"
            return True, message, full_path
        except Exception as e:
            self.logger.error(f"Error saving STL: {str(e)}")
            return False, f"Error saving STL: {str(e)}", ""
        
    def create_source(self, source_type):
        """
        Create a new geometric source
        
        Args:
            source_type: Type of source to create (Sphere, Cone, etc.)
            
        Returns:
            tuple: (success, message, source, source_name)
        """
        try:
            from paraview.simple import GetActiveView, Show
            source = None
            source_type = source_type.lower()
            
            if source_type == "sphere":
                from paraview.simple import Sphere
                source = Sphere()
            elif source_type == "cone":
                from paraview.simple import Cone
                source = Cone()
            elif source_type == "cylinder":
                from paraview.simple import Cylinder
                source = Cylinder()
            elif source_type == "plane":
                from paraview.simple import Plane
                source = Plane()
            elif source_type == "box":
                from paraview.simple import Box
                source = Box()
            else:
                return False, f"Unsupported source type: {source_type}", None, ""
            
            view = GetActiveView()
            Show(source, view)
            
            # Get the source name using the helper function
            source_name = self._get_source_name(source)
            
            return True, f"Created {source_type} source", source, source_name
        except Exception as e:
            self.logger.error(f"Error creating source: {str(e)}")
            return False, f"Error creating source: {str(e)}", None, ""
    
    def set_active_source(self, name):
        """
        Set the active pipeline object by matching its registered name. Use this function to set active source so that the computation is applied to the correct objects in paraview object hiearchy. 

        Args:
            name (str): The name of the pipeline object, e.g. "Slice1" or "Contour1".
                        Typically, ParaView registers pipeline objects using this sort of naming.

        Returns:
            tuple: (success: bool, message: str)
        """
        try:
            from paraview.simple import GetSources, SetActiveSource

            sources_dict = GetSources()  # Returns a dict: { (name, ""), proxyObject }, etc.
            if not sources_dict:
                return False, "No sources available in the pipeline."

            # Attempt exact or partial match:
            # Option A: Exact match on the first element of the key
            # Option B: A more flexible approach scanning all source names
            matches = []
            for (source_key, proxy) in sources_dict.items():
                # source_key is typically (registeredName, fileNameOrOtherString)
                if source_key[0] == name:
                    SetActiveSource(proxy)
                    return True, f"Active source set to '{source_key[0]}'"
                # Alternatively, you could allow partial or case-insensitive matches:
                # if name.lower() in source_key[0].lower():
                #     matches.append((source_key[0], proxy))

            return False, f"No source found with the name '{name}'."
        except Exception as e:
            self.logger.error(f"Error in set_active_source: {str(e)}")
            return False, f"Error setting active source: {str(e)}"

    def get_active_source_names_by_type(self, source_type=None):
        """
        Get a list of source names filtered by their type.
        
        Args:
            source_type (str, optional): Filter sources by type (e.g., 'Sphere', 'Contour', etc.).
                                      If None, returns all sources.
        
        Returns:
            tuple: (success: bool, message: str, source_names: list)
        """
        try:
            from paraview.simple import GetSources
            
            sources_dict = GetSources()
            if not sources_dict:
                return True, "No sources available in the pipeline.", []
            
            result_sources = []
            
            for (source_key, proxy) in sources_dict.items():
                proxy_type = proxy.__class__.__name__
                
                # If source_type is None or matches the proxy type, add to results
                if source_type is None or source_type.lower() in proxy_type.lower():
                    result_sources.append(source_key[0])
            
            if not result_sources and source_type:
                message = f"No sources of type '{source_type}' found in the pipeline."
            elif not result_sources:
                message = "No sources found in the pipeline."
            else:
                message = f"Found {len(result_sources)} source(s)" + (f" of type '{source_type}'" if source_type else "")
            
            return True, message, result_sources
            
        except Exception as e:
            self.logger.error(f"Error getting source names by type: {str(e)}")
            return False, f"Error getting source names by type: {str(e)}", []


    def create_isosurface(self, value, field=None):
        """
        Create or update an isosurface visualization of the loaded volume data.
        If an isosurface filter already exists (stored in self.isosurface_filter),
        update its isovalue and contour parameters. Otherwise, create a new filter.

        Args:
            value: Isovalue.
            field: Optional field name to contour by.

        Returns:
            tuple: (success: bool, message: str, contour_proxy, contour_name: str)
        """
        try:
            from paraview.simple import (
                GetActiveView, SetActiveSource, Contour, Show, GetActiveSource
            )

            # Use the originally loaded source if available; fall back to the active source.
            base_source = self.original_source or GetActiveSource()
            if not base_source:
                return False, "Error: No active source. Load data first.", None, ""

            # Determine whether to update an existing isosurface or create a new one.
            if hasattr(self, 'isosurface_filter') and self.isosurface_filter:
                contour = self.isosurface_filter
                contour.Isosurfaces = [value]
                if field:
                    contour.ContourBy = ['POINTS', field]
                message = f"Updated isosurface to value {value}"
            else:
                contour = Contour(Input=base_source)
                contour.Isosurfaces = [value]
                if field:
                    contour.ContourBy = ['POINTS', field]
                self.isosurface_filter = contour
                message = f"Created isosurface at value {value}"

            # Show the contour in the active view
            view = GetActiveView()
            Show(contour, view)

            # Optionally reset active source to the original data
            SetActiveSource(base_source)

            # Get the source name using the helper function
            contour_name = self._get_source_name(contour)

            # Return a 4-tuple including the name
            return True, message, contour, contour_name

        except Exception as e:
            self.logger.error(f"Error creating/updating isosurface: {str(e)}")
            return False, f"Error creating/updating isosurface: {str(e)}", None, ""

    def compute_surface_area(self):
        """
        Compute the surface area of the ACTIVE source.

        IMPORTANT: This assumes the active pipeline object is a surface mesh.
        If the active pipeline is still a volumetric dataset, you won't get
        a valid 'Area' array. For example, you might want to call:
        1) extract_surface()
        2) [SetActiveSource(...) for the extracted surface]
        3) compute_surface_area()

        Returns:
            tuple: (success: bool, message: str, area_value: float)
        """
        try:
            from paraview.simple import GetActiveSource, IntegrateVariables
            import paraview.servermanager as sm

            source = GetActiveSource()
            if not source:
                return False, "Error: No active source. Load data first.", 0.0

            # IntegrateVariables on a surface dataset yields an 'Area' array
            integrate_filter = IntegrateVariables(Input=source)
            integrate_filter.UpdatePipeline()

            # Fetch integrated results
            integrated_data = sm.Fetch(integrate_filter)
            if not integrated_data:
                return False, "Error: Could not fetch integrated data from server.", 0.0

            # Look for 'Area' array in CellData
            area_array = integrated_data.GetCellData().GetArray("Area")
            if not area_array:
                return False, (
                    "No 'Area' array found. Are you sure this is a surface dataset?"
                ), 0.0

            area_value = area_array.GetValue(0)
            return True, f"Computed surface area: {area_value}", area_value

        except Exception as e:
            self.logger.error(f"Error computing surface area: {str(e)}")
            return False, f"Error computing surface area: {str(e)}", 0.0

            # The integrated filter typically stores one value (the total area) in index 0
            total_area = area_array.GetValue(0)

            return (True, "Successfully computed surface area.", total_area)

        except Exception as e:
            self.logger.error(f"Error computing surface area: {str(e)}")
            return (False, f"Error computing surface area: {str(e)}", None)
        
    def create_slice(self, origin_x=None, origin_y=None, origin_z=None,
                 normal_x=0, normal_y=0, normal_z=1):
        """
        Create a slice through the loaded volume data.

        Args:
            origin_x, origin_y, origin_z: Coordinates for slice origin (default: center of dataset).
                                        If None, it uses the dataset's center.
            normal_x, normal_y, normal_z: Normal of the slice plane (default: [0, 0, 1]).

        Returns:
            tuple: (success: bool, message: str, slice_filter, slice_name: str)
        """
        try:
            from paraview.simple import (
                GetActiveView, SetActiveSource, Slice, Show, GetActiveSource
            )

            base_source = self.original_source or GetActiveSource()
            if not base_source:
                return False, "Error: No active source. Load data first.", None, None

            # If origin is unspecified, use the center of the dataset
            if origin_x is not None and origin_y is not None and origin_z is not None:
                origin = [origin_x, origin_y, origin_z]
            else:
                info = base_source.GetDataInformation()
                bounds = info.GetBounds()
                origin = [
                    (bounds[0] + bounds[1]) / 2,
                    (bounds[2] + bounds[3]) / 2,
                    (bounds[4] + bounds[5]) / 2
                ]

            normal = [normal_x, normal_y, normal_z]

            # Create and configure the slice filter
            slice_filter = Slice(Input=base_source)
            slice_filter.SliceType = 'Plane'
            slice_filter.SliceType.Origin = origin
            slice_filter.SliceType.Normal = normal

            # Show the new slice in the view
            view = GetActiveView()
            Show(slice_filter, view)

            # (Optional) reset the active source to the original volume
            SetActiveSource(base_source)

            # Get the source name using the helper function
            slice_name = self._get_source_name(slice_filter)

            message = (
                f"Created slice with origin {origin} and normal {normal}. "
                f"Slice name is: {slice_name}"
            )
            return True, message, slice_filter, slice_name

        except Exception as e:
            self.logger.error(f"Error creating slice: {str(e)}")
            return False, f"Error creating slice: {str(e)}", None, None

        
    def create_volume_rendering(self, enable=True):
        """
        Toggle volume rendering for the loaded volume data.
        
        Args:
            enable (bool): Whether to enable (True) or disable (False) volume rendering.
                          If True, shows volume rendering.
                          If False, hides the volume but preserves the volume representation.
        
        Returns:
            tuple: (success, message, source_name)
        """
        try:
            from paraview.simple import GetActiveView, SetActiveSource, GetDisplayProperties

            if not self.original_source:
                return False, "Error: No original data loaded. Load data first.", None

            # Force the original volume data to be active
            SetActiveSource(self.original_source)
            view = GetActiveView()
            display = GetDisplayProperties(self.original_source, view)

            # Get the current representation type
            current_rep = display.GetRepresentationType() if hasattr(display, 'GetRepresentationType') else None
            
            if enable:
                # Switch to Volume representation if not already
                if current_rep != 'Volume':
                    display.SetRepresentationType('Volume')
                # Make sure it's visible
                display.Visibility = 1
                status_message = "Volume rendering enabled"
            else:
                # If currently in Volume mode, make it invisible
                # but don't change the representation type
                if current_rep == 'Volume':
                    display.Visibility = 0
                    status_message = "Volume rendering hidden (representation preserved)"
                else:
                    # If not in Volume mode, just report current state
                    status_message = f"Volume rendering already disabled (current representation: {current_rep})"

            # Get the source name using the helper function
            source_name = self._get_source_name(self.original_source)

            return True, status_message, source_name

        except Exception as e:
            self.logger.error(f"Error toggling volume rendering: {str(e)}")
            return False, f"Error toggling volume rendering: {str(e)}", None

    def toggle_visibility(self, enable=True):
        """
        Toggle visibility for the current source.
        
        Args:
            enable (bool): Whether to enable (True) or disable (False) visibility of the current source.
                          If True, shows the current source.
                          If False, hides the current source.
        
        Returns:
            tuple: (success, message, source_name)
        """
        try:
            from paraview.simple import GetActiveView, SetActiveSource, GetDisplayProperties

            if not GetActiveSource():
                return False, "Error: No data selected. Load data first.", None

            view = GetActiveView()
            display = GetDisplayProperties(GetActiveSource(), view)
            
            if enable:
                display.Visibility = 1
                status_message = "Element was made visibile"
            else:
                display.Visibility = 0
                status_message = "Rendering hidden (representation preserved)"

            # Get the source name using the helper function
            source_name = self._get_source_name(GetActiveSource())

            return True, status_message, source_name

        except Exception as e:
            self.logger.error(f"Error toggling visibility: {str(e)}")
            return False, f"Error toggling visibility: {str(e)}", None
    
    def color_by(self, field, component=-1):
        """
        Color the active visualization by a specific field.
        This function first checks if the active source can be colored by fields
        (i.e., it's a dataset with arrays) before attempting to apply colors.
        
        Args:
            field: Field name to color by.
            component: Component to color by (-1 for magnitude).
            
        Returns:
            tuple: (success, message)
        """
        try:
            from paraview.simple import GetActiveSource, GetActiveView, GetDisplayProperties, ColorBy
            
            source = GetActiveSource()
            if not source:
                return False, "Error: No active source. Load data first."
            
            view = GetActiveView()
            display = GetDisplayProperties(source, view)
            
            # Check if the current representation type can be colored by arrays
            # Some representations (like 'Outline') cannot be colored by data arrays
            rep_type = display.GetRepresentationType() if hasattr(display, 'GetRepresentationType') else None
            if rep_type in ['Outline', 'Wireframe']:
                return False, f"Error: The current representation type '{rep_type}' cannot be colored by fields. Try changing to 'Surface' or 'Volume' first."
            
            # Get data information directly from the source
            data_info = source.GetDataInformation()
            point_info = data_info.GetPointDataInformation()
            cell_info = data_info.GetCellDataInformation()
            
            # Check if the active source has data arrays
            if (point_info.GetNumberOfArrays() == 0 and 
                cell_info.GetNumberOfArrays() == 0):
                return False, "Error: The active source does not have any data arrays to color by."
            
            # Try to find the requested field
            field_available = False
            field_location = None
            
            # Check point data arrays
            for i in range(point_info.GetNumberOfArrays()):
                array_info = point_info.GetArrayInformation(i)
                if array_info.GetName() == field:
                    ColorBy(display, ('POINTS', field), component)
                    field_available = True
                    field_location = 'POINTS'
                    break
            
            # Check cell data arrays if not found in point data
            if not field_available:
                for i in range(cell_info.GetNumberOfArrays()):
                    array_info = cell_info.GetArrayInformation(i)
                    if array_info.GetName() == field:
                        ColorBy(display, ('CELLS', field), component)
                        field_available = True
                        field_location = 'CELLS'
                        break
            
            if not field_available:
                # Build a list of available fields for better error reporting
                available_fields = []
                for i in range(point_info.GetNumberOfArrays()):
                    array_info = point_info.GetArrayInformation(i)
                    available_fields.append(f"{array_info.GetName()} (POINTS)")
                for i in range(cell_info.GetNumberOfArrays()):
                    array_info = cell_info.GetArrayInformation(i)
                    available_fields.append(f"{array_info.GetName()} (CELLS)")
                
                fields_str = ", ".join(available_fields)
                return False, f"Error: Field '{field}' not found. Available fields are: {fields_str}"
            
            # Rescale the color map to show the full data range
            display.RescaleTransferFunctionToDataRange(True)
            return True, f"Colored by field: '{field}' from {field_location}"
        except Exception as e:
            self.logger.error(f"Error coloring by field: {str(e)}")
            return False, f"Error coloring by field: {str(e)}"

    
    def set_color_map(self, preset_name="Blue-Red"):
        """
        Set the color map (lookup table) for the current visualization.
        
        Args:
            preset_name: Name of the color map preset.
                        Available presets include (but are not limited to):
                        - Blue-Red
                        - Cool to Warm
                        - Viridis
                        - Plasma
                        - Magma
                        - Inferno
                        - Rainbow
                        - Grayscale
                        
        Returns:
            tuple: (success, message)
        """
        try:
            from paraview.simple import GetActiveSource, GetActiveView, GetDisplayProperties, ApplyPreset
            source = GetActiveSource()
            if not source:
                return False, "Error: No active source. Load data first."
            
            view = GetActiveView()
            display = GetDisplayProperties(source, view)
            
            color_tf = display.LookupTable
            if not color_tf:
                return False, "Error: No active color transfer function"
            
            # Apply the requested preset to the color transfer function.
            ApplyPreset(color_tf, preset_name, True)
            
            available_presets = "Blue-Red, Cool to Warm, Viridis, Plasma, Magma, Inferno, Rainbow, Grayscale"
            return True, f"Applied color map preset: {preset_name}. Available presets include: {available_presets}"
        except Exception as e:
            self.logger.error(f"Error setting color map: {str(e)}")
            return False, f"Error setting color map: {str(e)}"

    def get_histogram(self, field=None, num_bins=256, data_location="POINTS"):
        """
        Compute and retrieve histogram data for a field in the active data source.
        This function is designed to work with volume sources. By default it uses the
        point data arrays (data_location="POINTS"), but you can specify "CELLS" if your
        volume source stores scalars on cells.

        If no field is provided and the active source contains exactly one available numeric 
        field in the specified data location, that field is automatically used. If multiple 
        arrays exist, the user must specify which field to use.

        Args:
            field (str, optional): The name of the field for which the histogram is computed.
            num_bins (int, optional): Number of histogram bins (default is 10).
            data_location (str, optional): Specify "POINTS" (default) or "CELLS" to indicate the source of the data.
            
        Returns:
            tuple: (success (bool), message (str), histogram_data (list of tuples))
                histogram_data is a list of tuples (bin_center, frequency) representing the computed histogram.

        Note:
            This function uses the Histogram filter from paraview.simple and updates the pipeline.
            Since direct assignment to properties like 'NumberOfBins' is disallowed, the code retrieves
            the proper property (either "NumberOfBins" or "BinCount") via GetProperty() and sets it via SetElement().
        """
        try:
            from paraview.simple import GetActiveSource, Histogram, UpdatePipeline, servermanager
            # Prefer the originally loaded volume so this tool stays idempotent
            # (a previous get_histogram may have left a Histogram filter as the
            # active source, which is a vtkTable and has no point/cell data).
            source = self.original_source or GetActiveSource()
            if not source:
                return False, "Error: No active source. Load data first.", None

            # If the chosen source is itself a Histogram filter (or anything
            # that lacks the expected data location), walk back through Inputs
            # until we find a real dataset.
            def _walk_back(s, depth=0):
                if depth > 5 or s is None:
                    return s
                cls = s.__class__.__name__
                if "Histogram" in cls or "PlotOver" in cls or "Integrate" in cls:
                    inp = getattr(s, "Input", None)
                    if inp is not None:
                        return _walk_back(inp, depth + 1)
                return s
            source = _walk_back(source)

            # Obtain the data information from the specified location.
            data_info = source.GetDataInformation()
            data_location = data_location.upper()
            if data_location == "CELLS":
                array_info_obj = data_info.GetCellDataInformation()
            else:
                array_info_obj = data_info.GetPointDataInformation()
            num_arrays = array_info_obj.GetNumberOfArrays()

            # Automatically determine the field if not provided.
            if field is None:
                if num_arrays == 1:
                    field = array_info_obj.GetArrayInformation(0).GetName()
                else:
                    available_arrays = []
                    for i in range(num_arrays):
                        available_arrays.append(array_info_obj.GetArrayInformation(i).GetName())
                    return (
                        False,
                        "Error: Multiple fields available. Please specify a field name. Available arrays: " +
                        ", ".join(available_arrays),
                        None
                    )

            # Reuse a cached Histogram filter if we created one previously,
            # otherwise create a new one. This keeps the pipeline clean across
            # repeated get_histogram calls.
            if getattr(self, "_histogram_filter", None) is not None:
                hist_filter = self._histogram_filter
                hist_filter.Input = source
            else:
                hist_filter = Histogram(Input=source)
                self._histogram_filter = hist_filter
            # Set the input array from the chosen location (POINTS or CELLS).
            hist_filter.SelectInputArray = [data_location, field]

            # Set the number of bins via GetProperty to avoid creating new attributes.
            nbins_prop = hist_filter.GetProperty("NumberOfBins")
            if nbins_prop is None:
                nbins_prop = hist_filter.GetProperty("BinCount")
            if nbins_prop is None:
                return False, "Error: Histogram filter does not have a 'NumberOfBins' or 'BinCount' property.", None
            nbins_prop.SetElement(0, num_bins)

            # Update the pipeline to compute the histogram.
            UpdatePipeline()

            # Fetch the computed histogram (returned as a vtkTable).
            hist_table = servermanager.Fetch(hist_filter)
            if hist_table.GetNumberOfRows() == 0:
                return False, "Histogram computation returned empty data.", None

            # Try to extract histogram data assuming columns named "bin_centers" and "bin_frequencies".
            bin_centers_col = hist_table.GetColumnByName("bin_centers")
            frequencies_col = hist_table.GetColumnByName("bin_frequencies")
            # Fallback: use the first two columns if the expected names do not exist.
            if not bin_centers_col or not frequencies_col:
                bin_centers_col = hist_table.GetColumn(0)
                frequencies_col = hist_table.GetColumn(1)

            histogram_data = []
            num_rows = hist_table.GetNumberOfRows()
            for i in range(num_rows):
                # Retrieve each value from the vtkArray for bin center and frequency.
                bin_center = bin_centers_col.GetValue(i)
                frequency = frequencies_col.GetValue(i)
                histogram_data.append((bin_center, frequency))

            return True, f"Histogram computed for field '{field}' in {data_location} with {num_bins} bins.", histogram_data

        except Exception as e:
            self.logger.error(f"Error computing histogram: {str(e)}")
            return False, f"Error computing histogram: {str(e)}", None


    def set_representation_type(self, rep_type):
        """
        Set the representation type for the active source.
        
        Args:
            rep_type: Representation type (Surface, Wireframe, Points, Volume, etc.)
            
        Returns:
            tuple: (success, message)
        """
        try:
            from paraview.simple import GetActiveSource, GetActiveView, GetDisplayProperties
            source = GetActiveSource()
            if not source:
                return False, "Error: No active source. Load data first."
            
            view = GetActiveView()
            display = GetDisplayProperties(source, view)
            
            display.SetRepresentationType(rep_type)
            
            return True, f"Set representation type to {rep_type}"
        except Exception as e:
            self.logger.error(f"Error setting representation type: {str(e)}")
            return False, f"Error setting representation type: {str(e)}"
    
    def edit_volume_opacity(self, field_name, opacity_points):
        """
        Edit ONLY the opacity transfer function for a given field, ensuring
        we pass only (value, alpha) pairs to ParaView.

        Args:
            field_name (str): The name of the field/array to modify.
            opacity_points (list of tuples): Each tuple must be (value, alpha).
                Example: [(0.0, 0.0), (50.0, 0.3), (100.0, 1.0)]

        Returns:
            tuple: (success: bool, message: str)
        """
        try:
            from paraview.simple import GetOpacityTransferFunction

            if not opacity_points:
                return False, "No opacity points provided."

            # Grab the opacity transfer function for the specified field
            opacity_tf = GetOpacityTransferFunction(field_name)
            if opacity_tf is None:
                return False, f"Could not find an opacity transfer function for field '{field_name}'."

            # Flatten the list of (value, alpha) into the format:
            # [val1, alpha1, midpoint1, sharpness1, val2, alpha2, midpoint2, sharpness2, ...]
            new_opacity_pts = []
            for val, alpha in opacity_points:
                new_opacity_pts.extend([val, alpha, 0.5, 0.0])  # midpoint=0.5, sharpness=0.0

            # Assign them to the piecewise function
            opacity_tf.Points = new_opacity_pts

            return True, f"Opacity transfer function updated for field '{field_name}'."

        except Exception as e:
            self.logger.error(f"Error editing opacity transfer function: {str(e)}")
            return False, f"Error editing opacity transfer function: {str(e)}"


    def set_color_map(self, field_name, color_points):
        """
        Sets the color transfer function for the given field (array) in ParaView.

        Args:
            field_name (str): The name of the field/array (as it appears in ParaView).
            color_points (list of (float, (float, float, float))):
                Each element should be a tuple: (value, (r, g, b))
                where value is the data value, and r, g, b are in [0, 1].

        Returns:
            tuple (success: bool, message: str)
        """
        try:
            from paraview.simple import GetColorTransferFunction

            if not color_points:
                return False, "No color points provided."

            # Retrieve/create the color transfer function for the specified field
            color_tf = GetColorTransferFunction(field_name)
            if color_tf is None:
                return False, f"Could not find or create a color transfer function for '{field_name}'."

            # Flatten the list into [value, R, G, B, value, R, G, B, ...]
            new_rgb_points = []
            for val, rgb in color_points:
                if len(rgb) != 3:
                    return False, f"Invalid RGB tuple for value {val}: {rgb}"
                r, g, b = rgb
                new_rgb_points.extend([val, r, g, b])

            # Update the color transfer function
            color_tf.RGBPoints = new_rgb_points

            # Optionally, you can rescale the transfer function based on min and max values
            # Example:
            # min_val = min([pt[0] for pt in color_points])
            # max_val = max([pt[0] for pt in color_points])
            # color_tf.RescaleTransferFunction(min_val, max_val)

            return True, f"Color transfer function updated for field '{field_name}'."

        except Exception as e:
            msg = f"Error setting color map: {str(e)}"
            return False, msg


    def get_pipeline(self):
        """
        Get the current pipeline structure.
        
        Returns:
            tuple: (success, message)
        """
        try:
            from paraview.simple import GetSources
            sources = GetSources()
            if not sources:
                return True, "Pipeline is empty. No sources found."
            
            response = "Current pipeline:\n"
            for name, source in sources.items():
                response += f"- {name[0]}: {source.__class__.__name__}\n"
            return True, response
        except Exception as e:
            self.logger.error(f"Error getting pipeline: {str(e)}")
            return False, f"Error getting pipeline: {str(e)}"
    
    def get_available_arrays(self):
        """
        Get a list of available arrays in the active source.

        Returns:
            tuple: (success, message)
        """
        try:
            from paraview.simple import GetActiveSource
            source = GetActiveSource()
            if not source:
                return False, "Error: No active source. Load data first."

            # Obtain comprehensive data information from the source.
            data_info = source.GetDataInformation()
            point_info = data_info.GetPointDataInformation()
            cell_info  = data_info.GetCellDataInformation()

            response = "Available arrays:\n\nPoint data arrays:\n"
            if point_info:
                num_point_arrays = point_info.GetNumberOfArrays()
                for i in range(num_point_arrays):
                    # Get the array information for each point array.
                    array_info = point_info.GetArrayInformation(i)
                    array_name = array_info.GetName()  # Use GetName() rather than GetArrayName()
                    components = array_info.GetNumberOfComponents()
                    response += f"- {array_name} ({components} components)\n"
            else:
                response += "No point data arrays found.\n"

            response += "\nCell data arrays:\n"
            if cell_info:
                num_cell_arrays = cell_info.GetNumberOfArrays()
                for i in range(num_cell_arrays):
                    # Get the array information for each cell array.
                    array_info = cell_info.GetArrayInformation(i)
                    array_name = array_info.GetName()
                    components = array_info.GetNumberOfComponents()
                    response += f"- {array_name} ({components} components)\n"
            else:
                response += "No cell data arrays found.\n"

            return True, response
        except Exception as e:
            self.logger.error(f"Error getting available arrays: {str(e)}")
            return False, f"Error getting available arrays: {str(e)}"

    def create_stream_tracer(self, vector_field=None, base_source=None, point_center=None,
                            integration_direction="BOTH", 
                            initial_step_length=0.1,
                            maximum_stream_length=50.0,
                            number_of_streamlines=100,
                            point_radius=1.0,
                            tube_radius=0.1,
                            make_volume_transparent=True):
        """
        Create a stream tracer visualization for a vector volume with tube representation.

        Args:
            vector_field (str, optional): Name of the vector field to trace.
                                        If None, the function automatically selects
                                        the first array with more than one component.
            base_source (optional): The data source (volume) on which to perform stream tracing.
                                If None, uses self.original_source or GetActiveSource().
            point_center (list, optional): Center coordinates [x, y, z] for the seed points.
                                        If None, the center of the volume's bounds is used.
            integration_direction (str): "FORWARD", "BACKWARD", or "BOTH" for integration.
            initial_step_length (float): The initial step size.
            maximum_stream_length (float): Maximum streamline length, beyond which integration terminates.
            number_of_streamlines (int): Number of seed points if a default seed is created.
            point_radius (float): Radius for the Point Cloud seed.
            tube_radius (float): Radius for the tube visualization.
            make_volume_transparent (bool): Whether to make the base volume transparent.

        Returns:
            tuple: (success (bool), message (str), tube filter proxy, tube_name (str))
        """
        try:
            from paraview.simple import (GetActiveSource, GetActiveView, StreamTracer, 
                                        Show, SetActiveSource, FindSource, Tube,
                                        GetDisplayProperties, ColorBy)

            # Determine the base source: use provided, or self.original_source, or the active source.
            if base_source is None:
                base_source = self.original_source or GetActiveSource()
            if not base_source:
                return False, "Error: No active source. Load data first.", None, ""

            # Log the base source name if available.
            base_source_name = None
            if hasattr(base_source, "SMProxy") and hasattr(base_source.SMProxy, "GetXMLName"):
                base_source_name = base_source.SMProxy.GetXMLName()
            else:
                base_source_name = str(base_source)
            self.logger.info(f"Using base source: {base_source_name}")

            # If vector_field is not provided, get the first available multi-component array.
            if vector_field is None:
                # Retrieve the data information and then its point data information.
                data_info = base_source.GetDataInformation()
                point_info = data_info.GetPointDataInformation()
                if point_info:
                    num_arrays = point_info.GetNumberOfArrays()
                    found = False
                    for i in range(num_arrays):
                        array_info = point_info.GetArrayInformation(i)
                        components = array_info.GetNumberOfComponents()
                        if components > 1:
                            vector_field = array_info.GetName()
                            found = True
                            self.logger.info(f"Automatically selected vector field: {vector_field}")
                            break
                    if not found:
                        if num_arrays > 0:
                            vector_field = point_info.GetArrayInformation(0).GetName()
                            self.logger.info(f"No multi-component array found; selected first array: {vector_field}")
                        else:
                            return False, "Error: No arrays found in the base source.", None, ""
                else:
                    return False, "Error: Could not retrieve point data information.", None, ""

            # Determine point center for the seed source
            center = point_center
            if center is None:
                data_info = base_source.GetDataInformation()
                bounds = data_info.GetBounds()  # Format: [xmin, xmax, ymin, ymax, zmin, zmax]
                center = [(bounds[0] + bounds[1]) / 2.0,
                        (bounds[2] + bounds[3]) / 2.0,
                        (bounds[4] + bounds[5]) / 2.0]
                self.logger.info(f"Using auto-calculated center point at {center}")

            # Create the stream tracer filter using Point Cloud seed type
            tracer = StreamTracer(Input=base_source, SeedType='Point Cloud')
            tracer.Vectors = ['POINTS', vector_field]
            tracer.IntegrationDirection = integration_direction
            tracer.InitialStepLength = initial_step_length
            tracer.MaximumStreamlineLength = maximum_stream_length
            
            # Configure the Point Cloud seed
            tracer.SeedType.Center = center
            tracer.SeedType.NumberOfPoints = number_of_streamlines
            tracer.SeedType.Radius = point_radius
            
            # Display the tracer result
            Show(tracer)
            
            # Create tube filter for better visualization
            tube = Tube(Input=tracer)
            tube.Radius = tube_radius
            
            # Show the tube filter
            # Display the tube with proper coloring
            tube_display = Show(tube)
            ColorBy(tube_display, ('POINTS', vector_field))

            # Make the base source transparent if requested
            if make_volume_transparent:
                try:
                    base_display = GetDisplayProperties(base_source)
                    base_display.Opacity = 0.3  # Set opacity to make volume transparent
                except Exception as e:
                    self.logger.warning(f"Could not make volume transparent: {str(e)}")
            
            # Set the active source to the tube filter
            SetActiveSource(tube)
            
            # Get the tube filter name using the helper function
            tube_name = self._get_source_name(tube)

            msg = f"Stream tracer with tubes created for vector field '{vector_field}' using base source '{base_source_name}'."
            return True, msg, tube, tube_name
        except Exception as e:
            self.logger.error(f"Error creating stream tracer: {str(e)}")
            return False, f"Error creating stream tracer: {str(e)}", None, ""

    def get_screenshot(self):
        """
        Capture a screenshot from the current view.
        
        Returns:
            tuple: (success, message, img_path)
        """
        try:
            from paraview.collaboration import processServerEvents
            import tempfile
            processServerEvents()
            from paraview import servermanager
            from paraview.simple import SetActiveView, RenderAllViews, SaveScreenshot, ResetCamera
            
            # Get the active render view from the GUI connection
            pxm = servermanager.ProxyManager()
            gui_view = None
            views = pxm.GetProxiesInGroup("views")
            for (group, name), view_proxy in views.items():
                if view_proxy.GetXMLName() == "RenderView":
                    gui_view = view_proxy
                    break
            
            if not gui_view:
                return False, (
                    "No render view found on the server. Load data (or call "
                    "Show) first so a RenderView exists, or connect a ParaView "
                    "GUI client."
                ), None
            
            # Set the found GUI view active
            SetActiveView(gui_view)
            RenderAllViews()
            
            import tempfile
            with tempfile.NamedTemporaryFile(suffix='.png', delete=False) as tmp:
                temp_path = tmp.name
            
            # Force a real viewport size so headless renders aren't tiny PNGs
            # the LLM can't read.
            gui_view.ViewSize = [1024, 768]
            SaveScreenshot(temp_path, gui_view, ImageResolution=[1024, 768])
            return True, "Screenshot captured", temp_path
        except Exception as e:
            self.logger.error(f"Error getting screenshot: {str(e)}")
            return False, f"Error getting screenshot: {str(e)}", None
    

    def rotate_camera(self, azimuth=30.0, elevation=0.0):
        """
        Rotate the camera by specified angles.
        
        Args:
            azimuth: Rotation around vertical axis in degrees.
            elevation: Rotation around horizontal axis in degrees.
            
        Returns:
            tuple: (success, message)
        """
        try:
            from paraview.simple import GetActiveView
            view = GetActiveView()
            if not view:
                return False, "Error: No active view."
            
            camera = view.GetActiveCamera()
            camera.Azimuth(azimuth)
            camera.Elevation(elevation)
            return True, f"Rotated camera by azimuth: {azimuth}, elevation: {elevation}"
        except Exception as e:
            self.logger.error(f"Error rotating camera: {str(e)}")
            return False, f"Error rotating camera: {str(e)}"
    
    def reset_camera(self):
        """
        Reset the camera to show all data.
        
        Returns:
            tuple: (success, message)
        """
        try:
            from paraview.simple import GetActiveView, ResetCamera
            view = GetActiveView()
            if not view:
                return False, "Error: No active view."
            ResetCamera(view)
            return True, "Camera reset"
        except Exception as e:
            self.logger.error(f"Error resetting camera: {str(e)}")
            return False, f"Error resetting camera: {str(e)}"
    

    def plot_over_line(self, point1=None, point2=None, resolution=100):
        """
        Sample data along a line between two points and return the sampled
        scalar values so the caller can do numerical analysis.

        Args:
            point1 (list/tuple or None): [x, y, z] of the start point.
                If None, uses the dataset's min corner.
            point2 (list/tuple or None): [x, y, z] of the end point.
                If None, uses the dataset's max corner.
            resolution (int): Number of sample points along the line.

        Returns:
            tuple: (success: bool, message: str, samples)
                samples is a list of dicts: [{"t": arc_length, "<field>": value, ...}, ...]
                where each dict contains arc_length and every available scalar
                array value at that sample point.
        """
        try:
            from paraview.simple import (
                GetActiveSource, PlotOverLine, UpdatePipeline, servermanager,
            )
            # Prefer the original loaded volume so the line samples the
            # raw scalar field, not a downstream filter.
            source = self.original_source or GetActiveSource()
            if not source:
                return False, "Error: No active source. Load data first.", None

            # Default endpoints to the data bounds diagonal.
            if point1 is None or point2 is None:
                bounds = source.GetDataInformation().GetBounds()
                if point1 is None:
                    point1 = [bounds[0], bounds[2], bounds[4]]
                if point2 is None:
                    point2 = [bounds[1], bounds[3], bounds[5]]

            # Reuse a cached PlotOverLine filter so we don't pile up
            # PlotOverLine1, PlotOverLine2, ... in the pipeline.
            if getattr(self, "_plot_over_line_filter", None) is not None:
                pol = self._plot_over_line_filter
                pol.Input = source
            else:
                pol = PlotOverLine(Input=source)
                self._plot_over_line_filter = pol
            pol.Point1 = list(point1)
            pol.Point2 = list(point2)
            pol.Resolution = int(resolution)

            UpdatePipeline()
            output = servermanager.Fetch(pol)
            if output is None:
                return False, "PlotOverLine returned no output.", None

            # PlotOverLine returns a vtkPolyData (or vtkTable depending on
            # ParaView version); handle both. Sampled scalar fields live in
            # PointData on vtkPolyData.
            samples = []
            col_names = []
            if hasattr(output, "GetPointData") and output.GetPointData() is not None:
                pd = output.GetPointData()
                n_arr = pd.GetNumberOfArrays()
                col_names = [pd.GetArrayName(i) for i in range(n_arr)]
                n_pts = output.GetNumberOfPoints()
                for i in range(n_pts):
                    row = {}
                    for j in range(n_arr):
                        name = col_names[j]
                        if name == "vtkValidPointMask":
                            continue
                        arr = pd.GetArray(j)
                        try:
                            row[name] = float(arr.GetTuple1(i))
                        except Exception:
                            pass
                    # Real coordinates of this sample, courtesy of the polyline geometry
                    pt = output.GetPoint(i)
                    row["_x"] = float(pt[0])
                    row["_y"] = float(pt[1])
                    row["_z"] = float(pt[2])
                    samples.append(row)
                n_rows = n_pts
            elif hasattr(output, "GetNumberOfRows"):
                n_rows = output.GetNumberOfRows()
                n_cols = output.GetNumberOfColumns()
                col_names = [output.GetColumnName(i) for i in range(n_cols)]
                for i in range(n_rows):
                    row = {}
                    for j, name in enumerate(col_names):
                        if name == "vtkValidPointMask":
                            continue
                        col = output.GetColumn(j)
                        try:
                            row[name] = float(col.GetValue(i))
                        except Exception:
                            pass
                    samples.append(row)
            else:
                return False, f"Unexpected output type: {type(output).__name__}", None

            if not samples:
                return False, "PlotOverLine returned no rows.", None

            msg = (
                f"Sampled {len(samples)} points along line "
                f"{list(point1)} -> {list(point2)}. "
                f"Columns: {', '.join(col_names)}"
            )
            return True, msg, samples
        except Exception as e:
            self.logger.error(f"Error creating plot over line: {str(e)}")
            return False, f"Error creating plot over line: {str(e)}", None

    def find_connected_components(self, threshold, field=None, location="POINTS"):
        """
        Threshold the active scalar field above `threshold`, then find all
        connected components and return per-region centroid + bounds + size.

        This is the right tool for "how many distinct objects are there?"
        and "where are the blobs?" tasks — far more efficient than line
        probing for enumerating multiple peaks.

        Args:
            threshold (float): Lower bound; only voxels with value >= threshold
                are considered "in" an object.
            field (str, optional): Scalar field name. If None, the first
                available array in `location` is auto-selected.
            location (str): "POINTS" (default) or "CELLS".

        Returns:
            tuple: (success, message, regions) where regions is a list of dicts:
                {region_id, n_points, centroid: [x,y,z], bounds: [x0,x1,y0,y1,z0,z1]}
        """
        try:
            from paraview.simple import (
                Threshold, Connectivity, UpdatePipeline,
                GetActiveSource, servermanager,
            )
            source = self.original_source or GetActiveSource()
            if not source:
                return False, "Error: No active source. Load data first.", None

            location = location.upper()
            di = source.GetDataInformation()
            ai = (di.GetPointDataInformation()
                  if location == "POINTS"
                  else di.GetCellDataInformation())
            if ai is None or ai.GetNumberOfArrays() == 0:
                return False, f"No {location.lower()} data arrays on active source.", None
            if field is None:
                field = ai.GetArrayInformation(0).GetName()

            # Threshold filter — reuse a cached one to keep the pipeline clean
            if getattr(self, "_threshold_filter", None) is not None:
                thr = self._threshold_filter
                thr.Input = source
            else:
                thr = Threshold(Input=source)
                self._threshold_filter = thr
            thr.Scalars = [location, field]

            # ParaView 5.10+ uses LowerThreshold/UpperThreshold + ThresholdMethod.
            # Get a sane upper bound from the data range.
            field_info = None
            for i in range(ai.GetNumberOfArrays()):
                ainfo = ai.GetArrayInformation(i)
                if ainfo.GetName() == field:
                    field_info = ainfo
                    break
            try:
                rng = field_info.GetComponentRange(0) if field_info else (float(threshold), float(threshold) * 1e6)
            except Exception:
                rng = (float(threshold), 1e30)
            upper_bound = max(float(rng[1]), float(threshold) + 1.0)

            try:
                thr.LowerThreshold = float(threshold)
                thr.UpperThreshold = upper_bound
                # ThresholdMethod = "Between" (id 0) selects values inside [lower, upper]
                if hasattr(thr, "ThresholdMethod"):
                    thr.ThresholdMethod = "Between"
            except Exception:
                # Fallback for older API
                thr.ThresholdRange = [float(threshold), upper_bound]

            # Connectivity filter
            if getattr(self, "_connectivity_filter", None) is not None:
                conn = self._connectivity_filter
                conn.Input = thr
            else:
                conn = Connectivity(Input=thr)
                self._connectivity_filter = conn
            try:
                conn.ExtractionMode = "Extract All Regions"
            except Exception:
                pass
            try:
                conn.ColorRegions = 1
            except Exception:
                pass

            UpdatePipeline()
            output = servermanager.Fetch(conn)
            if output is None:
                return False, "Connectivity filter returned no output.", None

            # Pull RegionId out of point or cell data
            region_arr = None
            pd = output.GetPointData() if hasattr(output, "GetPointData") else None
            cd = output.GetCellData() if hasattr(output, "GetCellData") else None
            if pd is not None:
                region_arr = pd.GetArray("RegionId")
            if region_arr is None and cd is not None:
                region_arr = cd.GetArray("RegionId")

            from collections import defaultdict
            region_pts = defaultdict(list)
            n_pts = output.GetNumberOfPoints()
            if region_arr is not None and pd is not None and pd.GetArray("RegionId") is not None:
                # Per-point RegionId — easy case
                for i in range(n_pts):
                    rid = int(region_arr.GetValue(i))
                    p = output.GetPoint(i)
                    region_pts[rid].append((float(p[0]), float(p[1]), float(p[2])))
            elif region_arr is not None and cd is not None:
                # Per-cell RegionId — assign each cell's points to its region
                n_cells = output.GetNumberOfCells()
                for ci in range(n_cells):
                    rid = int(region_arr.GetValue(ci))
                    cell = output.GetCell(ci)
                    for pi in range(cell.GetNumberOfPoints()):
                        p = output.GetPoint(cell.GetPointId(pi))
                        region_pts[rid].append((float(p[0]), float(p[1]), float(p[2])))
            else:
                return False, "Connectivity output had no RegionId array.", None

            regions = []
            for rid in sorted(region_pts.keys()):
                pts = region_pts[rid]
                n = len(pts)
                if n == 0:
                    continue
                cx = sum(p[0] for p in pts) / n
                cy = sum(p[1] for p in pts) / n
                cz = sum(p[2] for p in pts) / n
                xs = [p[0] for p in pts]
                ys = [p[1] for p in pts]
                zs = [p[2] for p in pts]
                regions.append({
                    "region_id": rid,
                    "n_points": n,
                    "centroid": [cx, cy, cz],
                    "bounds": [min(xs), max(xs), min(ys), max(ys), min(zs), max(zs)],
                })

            msg = (f"Found {len(regions)} connected component(s) above "
                   f"threshold={threshold} on field '{field}' ({location})")
            return True, msg, regions

        except Exception as e:
            self.logger.error(f"Error in find_connected_components: {str(e)}")
            return False, f"Error finding connected components: {str(e)}", None

    def warp_by_vector(self, vector_field=None, scale_factor=1.0):
        """
        Apply the 'Warp By Vector' filter to the active source.

        Args:
            vector_field (str, optional): The name of the vector field to use for warping. If None, the first available vector field will be used.
            scale_factor (float, optional): The scale factor for the warp (default: 1.0).

        Returns:
            tuple: (success: bool, message: str, warp_filter)
        """
        try:
            from paraview.simple import GetActiveSource, WarpByVector, Show, GetActiveView
            source = GetActiveSource()
            if not source:
                return False, "Error: No active source. Load data first.", None

            # If vector_field is not specified, try to auto-detect a vector field
            if vector_field is None:
                data_info = source.GetDataInformation()
                point_info = data_info.GetPointDataInformation()
                num_arrays = point_info.GetNumberOfArrays()
                found = False
                for i in range(num_arrays):
                    array_info = point_info.GetArrayInformation(i)
                    if array_info.GetNumberOfComponents() > 1:
                        vector_field = array_info.GetName()
                        found = True
                        break
                if not found:
                    return False, "No vector field found in the active source.", None

            # Create the WarpByVector filter
            warp_filter = WarpByVector(Input=source)
            warp_filter.Vectors = ['POINTS', vector_field]
            warp_filter.ScaleFactor = scale_factor

            # Show the result in the active view
            view = GetActiveView()
            Show(warp_filter, view)

            return True, f"Warp by vector applied using field '{vector_field}' with scale factor {scale_factor}.", warp_filter
        except Exception as e:
            self.logger.error(f"Error creating warp by vector: {str(e)}")
            return False, f"Error creating warp by vector: {str(e)}", None
