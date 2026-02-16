##############################################################################
# Copyright by The HDF Group.                                                #
# All rights reserved.                                                       #
#                                                                            #
# This file is part of the HDF Compass Viewer. The full HDF Compass          #
# copyright notice, including terms governing use, modification, and         #
# terms governing use, modification, and redistribution, is contained in     #
# the file COPYING, which can be found at the root of the source code        #
# distribution tree.  If you do not have access to this file, you may        #
# request a copy from help@hdfgroup.org.                                     #
##############################################################################
"""
Implements a viewer frame for compass_model.Array.
"""
from __future__ import absolute_import, division, print_function, unicode_literals

import wx
import wx.grid
from wx.lib.newevent import NewCommandEvent

import numpy as np

import os
import logging
import numpy

log = logging.getLogger(__name__)

from ..frame import NodeFrame
from .plot import LinePlotFrame, LineXYPlotFrame, ContourPlotFrame

class ArrayPanel(wx.Panel):
    """ Simple array panel with grid display """
    def __init__(self, parent, node):
        super(ArrayPanel, self).__init__(parent)
        self.node = node

        # Create a simple grid to show the array data
        self.grid = wx.grid.Grid(self)

        # Set up grid based on array shape
        try:
            if len(node.shape) == 0:  # scalar
                self.grid.CreateGrid(1, 1)
                try:
                    value = node[()]
                    self.grid.SetCellValue(0, 0, str(value))
                except Exception as e:
                    self.grid.SetCellValue(0, 0, f"<Error: {e}>")
            elif len(node.shape) == 1:  # 1D array
                rows = min(node.shape[0], 1000)  # Limit display for performance
                self.grid.CreateGrid(rows, 1)
                try:
                    data = node[:rows]
                    for i in range(rows):
                        try:
                            value = data[i]
                            # Handle numpy types and bytes
                            if hasattr(value, 'item'):
                                value = value.item()
                            if isinstance(value, bytes):
                                value = value.decode('utf-8', errors='replace')
                            self.grid.SetCellValue(i, 0, str(value))
                        except Exception as e:
                            self.grid.SetCellValue(i, 0, f"<Error: {e}>")
                except Exception as e:
                    # If we can't read the data at all, show an error message
                    self.grid.SetCellValue(0, 0, f"<Error reading data: {e}>")
            else:  # 2D or higher
                rows = min(node.shape[0], 100)
                cols = min(node.shape[1], 100) if len(node.shape) > 1 else 1
                self.grid.CreateGrid(rows, cols)
                if len(node.shape) == 2:
                    try:
                        data = node[:rows, :cols]
                        for i in range(rows):
                            for j in range(cols):
                                try:
                                    value = data[i, j]
                                    # Handle numpy types and bytes
                                    if hasattr(value, 'item'):
                                        value = value.item()
                                    if isinstance(value, bytes):
                                        value = value.decode('utf-8', errors='replace')
                                    self.grid.SetCellValue(i, j, str(value))
                                except Exception as e:
                                    self.grid.SetCellValue(i, j, f"<Error: {e}>")
                    except Exception as e:
                        # If we can't read the data at all, show an error message
                        self.grid.SetCellValue(0, 0, f"<Error reading data: {e}>")
        except Exception as e:
            # Fallback: create a minimal grid with error message
            if not hasattr(self, 'grid') or self.grid.GetNumberRows() == 0:
                self.grid.CreateGrid(1, 1)
            self.grid.SetCellValue(0, 0, f"<Error initializing grid: {e}>")

        # Layout
        sizer = wx.BoxSizer(wx.VERTICAL)
        sizer.Add(self.grid, 1, wx.EXPAND)
        self.SetSizer(sizer)
        
    def copy(self):
        """ Copy selected data to clipboard """
        pass  # TODO: Implement
        
    def export(self, path):
        """ Export data to CSV file """
        pass  # TODO: Implement
        
    def plot_data(self):
        """ Plot the data """
        try:
            from .plot import LinePlotFrame, ContourPlotFrame
            import numpy as np

            # Get the array frame to access the node data
            array_frame = self.GetParent()
            node = array_frame.node

            # Check if columns are selected in the grid
            selected_cols = self.grid.GetSelectedCols()
            selected_rows = self.grid.GetSelectedRows()

            # Get the raw data from the node
            data = node[:]

            # Handle column selection for plotting
            if len(selected_cols) > 0 and len(data.shape) >= 2:
                # Plot selected columns as separate line plots
                selected_data = []
                names = []
                for col in selected_cols:
                    if col < data.shape[1]:
                        selected_data.append(data[:, col])
                        names.append(f"Column {col}")

                if selected_data:
                    plot_frame = LinePlotFrame(selected_data, names, title=f"Plot (selected columns): {node.display_name}")
                    plot_frame.Show()
                    return

            # Handle row selection for plotting
            elif len(selected_rows) > 0 and len(data.shape) >= 1:
                # Plot selected rows as separate line plots
                selected_data = []
                names = []
                for row in selected_rows:
                    if row < data.shape[0]:
                        if len(data.shape) == 1:
                            # For 1D data, just plot the single value (not very useful)
                            selected_data.append([data[row]])
                        else:
                            # For 2D+ data, plot the row as a line
                            selected_data.append(data[row, :])
                        names.append(f"Row {row}")

                if selected_data:
                    plot_frame = LinePlotFrame(selected_data, names, title=f"Plot (selected rows): {node.display_name}")
                    plot_frame.Show()
                    return

            # No selection - plot all data using original logic
            # Determine plot type based on dimensionality
            if len(data.shape) == 0:
                wx.MessageBox("Cannot plot scalar data", "Plot Error", wx.OK | wx.ICON_ERROR)
                return
            elif len(data.shape) == 1:
                # 1D data - line plot
                plot_frame = LinePlotFrame([data], ["Data"], title=f"Plot: {node.display_name}")
            elif len(data.shape) == 2:
                # 2D data - contour plot
                plot_frame = ContourPlotFrame(data, ["Data"], title=f"Contour: {node.display_name}")
            else:
                # Multi-dimensional - plot first 2D slice
                if data.shape[0] == 1:
                    data_2d = data[0]
                else:
                    data_2d = data[0]
                plot_frame = ContourPlotFrame(data_2d, ["Data"], title=f"Contour (slice): {node.display_name}")

            plot_frame.Show()

        except Exception as e:
            wx.MessageBox(f"Error creating plot: {e}", "Plot Error", wx.OK | wx.ICON_ERROR)
        
    def plot_xy(self):
        """ Plot XY data """
        try:
            from .plot import LineXYPlotFrame
            import numpy as np

            # Get the array frame to access the node data
            array_frame = self.GetParent()
            node = array_frame.node

            # Check if columns are selected in the grid
            selected_cols = self.grid.GetSelectedCols()

            # Get the raw data from the node
            data = node[:]

            # Handle column selection for XY plotting
            if len(selected_cols) >= 2 and len(data.shape) >= 2:
                # Use first selected column as X and subsequent columns as Y series
                selected_cols = sorted(selected_cols)
                x_col = selected_cols[0]
                x_data = data[:, x_col]
                plot_data = [x_data]
                names = [f"Column {x_col}"]

                # Add all other selected columns as Y series
                for y_col in selected_cols[1:]:
                    if y_col < data.shape[1]:
                        plot_data.append(data[:, y_col])
                        names.append(f"Column {y_col}")

                plot_frame = LineXYPlotFrame(plot_data, names, title=f"XY Plot (selected columns): {node.display_name}")
                plot_frame.Show()
                return

            elif len(selected_cols) == 1 and len(data.shape) >= 2:
                # Only one column selected - use row indices as X
                col = selected_cols[0]
                if col < data.shape[1]:
                    x_data = np.arange(data.shape[0])
                    y_data = data[:, col]
                    plot_data = [x_data, y_data]
                    names = ["Row Index", f"Column {col}"]

                    plot_frame = LineXYPlotFrame(plot_data, names, title=f"XY Plot (Column {col}): {node.display_name}")
                    plot_frame.Show()
                    return

            # No selection or insufficient selection - use original logic
            # For XY plot, we need at least 2D data or 1D data that can be split
            if len(data.shape) == 0:
                wx.MessageBox("Cannot create XY plot from scalar data", "Plot Error", wx.OK | wx.ICON_ERROR)
                return
            elif len(data.shape) == 1:
                # 1D data - create X as indices and Y as data values
                x_data = np.arange(len(data))
                y_data = data
                plot_data = [x_data, y_data]
                names = ["Index", "Value"]
            elif len(data.shape) == 2:
                # 2D data - use first column as X and second as Y
                if data.shape[1] >= 2:
                    x_data = data[:, 0]
                    y_data = data[:, 1]
                    plot_data = [x_data, y_data]
                    names = ["Column 0", "Column 1"]
                else:
                    # Only one column - use indices as X
                    x_data = np.arange(data.shape[0])
                    y_data = data[:, 0]
                    plot_data = [x_data, y_data]
                    names = ["Row Index", "Value"]
            else:
                wx.MessageBox("XY plotting not supported for data with more than 2 dimensions", "Plot Error", wx.OK | wx.ICON_ERROR)
                return

            # Create XY plot
            plot_frame = LineXYPlotFrame(plot_data, names, title=f"XY Plot: {node.display_name}")
            plot_frame.Show()

        except Exception as e:
            wx.MessageBox(f"Error creating XY plot: {e}", "Plot Error", wx.OK | wx.ICON_ERROR)


# Indicates that the slicing selection may have changed.
# These events are emitted by the SlicerPanel.
ArraySlicedEvent, EVT_ARRAY_SLICED = NewCommandEvent()
ArraySelectionEvent, EVT_ARRAY_SELECTED = NewCommandEvent()

# Menu and button IDs
ID_VIS_MENU_PLOT = wx.NewId()
ID_VIS_MENU_PLOTXY = wx.NewId()
ID_VIS_MENU_COPY = wx.NewId()
ID_VIS_MENU_EXPORT = wx.NewId()

def gen_csv(data, delimiters):
    """ converts any N-dimensional array to a CSV-string """
    if(type(data) == numpy.ndarray or type(data) == list):
        return delimiters[0].join(map(lambda x: gen_csv(x,delimiters[1:]), data))
    else:
        return str(data)

class ArrayFrame(NodeFrame):
    """
    Top-level frame displaying objects of type compass_model.Array.

    From top to bottom, has:

    1. Toolbar (see ArrayFrame.init_toolbar)
    2. SlicerPanel, with controls for changing what's displayed.
    3. An ArrayGrid, which displays the data in a spreadsheet-like view.
    """

    last_open_csv = os.getcwd()
    csv_delimiters_copy = ['\n', '\t']
    csv_delimiters_export = ['\n', ',']

    def __init__(self, node, pos=None):
        """ Create a new array frame """
        super(ArrayFrame, self).__init__(node, size=(800, 400), title=node.display_name, pos=pos)

        # Create array-specific toolbar (replace any existing toolbar)
        existing_toolbar = self.GetToolBar()
        if existing_toolbar:
            existing_toolbar.Destroy()

        self.toolbar = self.CreateToolBar()
        self.toolbar.AddTool(wx.ID_COPY, "Copy", wx.ArtProvider.GetBitmap(wx.ART_COPY))
        self.toolbar.AddTool(wx.ID_SAVEAS, "Export", wx.ArtProvider.GetBitmap(wx.ART_FILE_SAVE))
        # Use specific plotting icons
        plot_icon_path = os.path.join(os.path.dirname(__file__), "..", "icons", "viz_plot_32.png")
        plot_xy_icon_path = os.path.join(os.path.dirname(__file__), "..", "icons", "viz_plot_xy_24.png")
        self.toolbar.AddTool(wx.ID_ZOOM_IN, "Plot Data", wx.Bitmap(plot_icon_path, wx.BITMAP_TYPE_PNG))
        self.toolbar.AddTool(wx.ID_ZOOM_OUT, "Plot XY", wx.Bitmap(plot_xy_icon_path, wx.BITMAP_TYPE_PNG))
        self.toolbar.Realize()

        # Initialize dimension selection for multidimensional arrays first
        if len(node.shape) > 2:
            self.rowSpin = wx.SpinCtrl(self, max=len(node.shape)-1, value="0", min=0)
            self.colSpin = wx.SpinCtrl(self, max=len(node.shape)-1, value="1", min=0)
            self.rowSpin.Bind(wx.EVT_SPINCTRL, self.on_dimSpin)
            self.colSpin.Bind(wx.EVT_SPINCTRL, self.on_dimSpin)
        else:
            # For 1D and 2D arrays, set default row/col indices
            self.rowSpin = None
            self.colSpin = None

        # Create the proper array view components first (before layout)
        # Create slicer panel for dimension control (parent must be ArrayFrame for indices property)
        has_names = node.dtype is not None and node.dtype.names is not None
        self.slicer = SlicerPanel(self, node.shape, has_names)

        # Layout components
        main_panel = wx.Panel(self)
        sizer = wx.BoxSizer(wx.VERTICAL)

        # Create grid for data display - need to pass ArrayFrame reference for table
        self.grid = ArrayGrid(main_panel, node, self.slicer, array_frame=self)

        # Add slicer if needed (reparent it to main_panel for proper layout)
        if len(node.shape) > (1 if has_names else 2):
            self.slicer.Reparent(main_panel)
            sizer.Add(self.slicer, 0, wx.EXPAND | wx.ALL, 5)

        # Add grid
        sizer.Add(self.grid, 1, wx.EXPAND | wx.ALL, 5)

        main_panel.SetSizer(sizer)
        self.view = main_panel

        # Bind events
        self.Bind(wx.EVT_TOOL, self.on_copy, id=wx.ID_COPY)
        self.Bind(wx.EVT_TOOL, self.on_export, id=wx.ID_SAVEAS)
        self.Bind(wx.EVT_TOOL, self.on_plot_data, id=wx.ID_ZOOM_IN)
        self.Bind(wx.EVT_TOOL, self.on_plot_xy, id=wx.ID_ZOOM_OUT)

        # Bind array events
        self.Bind(EVT_ARRAY_SLICED, self.on_sliced)
        self.Bind(EVT_ARRAY_SELECTED, self.on_selected)

        # Enable slicer controls after a short delay (wxPython Mac bug workaround)
        if hasattr(self.slicer, 'enable_spinctrls'):
            self.timer = wx.Timer(self)
            self.timer.Start(100, True)
            self.Bind(wx.EVT_TIMER, self.on_workaround_timer)

    def on_copy(self, evt):
        """ Handle copy toolbar button """
        # Get selected data and copy to clipboard
        data, names, line = self.get_selected_data()
        if data is not None:
            csv_data = gen_csv(data, self.csv_delimiters_copy)
            if wx.TheClipboard.Open():
                wx.TheClipboard.SetData(wx.TextDataObject(csv_data))
                wx.TheClipboard.Close()

    def on_export(self, evt):
        """ Handle export toolbar button """
        with wx.FileDialog(self, "Export array", wildcard="CSV files (*.csv)|*.csv",
                          style=wx.FD_SAVE | wx.FD_OVERWRITE_PROMPT) as dlg:
            if dlg.ShowModal() == wx.ID_OK:
                path = dlg.GetPath()
                data, names, line = self.get_selected_data()
                if data is not None:
                    csv_data = gen_csv(data, self.csv_delimiters_export)
                    with open(path, 'w') as f:
                        f.write(csv_data)

    def on_plot_data(self, evt):
        """ Handle plot data toolbar button """
        data, names, line = self.get_selected_data()
        if data is not None:
            if line:
                from .plot import LinePlotFrame
                plot_frame = LinePlotFrame(data, names, title=f"Plot: {self.node.display_name}")
                plot_frame.Show()
            else:
                from .plot import ContourPlotFrame
                plot_frame = ContourPlotFrame(data, names, title=f"Contour: {self.node.display_name}")
                plot_frame.Show()

    def on_plot_xy(self, evt):
        """ Handle plot XY toolbar button """
        data, names, line = self.get_selected_data()
        if data is not None and line:
            from .plot import LineXYPlotFrame
            # For XY plot, we need at least 2 data series
            if len(data) >= 2:
                plot_frame = LineXYPlotFrame(data, names, title=f"XY Plot: {self.node.display_name}")
                plot_frame.Show()
            else:
                wx.MessageBox("XY plotting requires at least 2 data series", "Plot Error", wx.OK | wx.ICON_ERROR)

    def on_selected(self, evt):
        """ User has chosen to display a different part of the dataset. """
        idx = 0
        for x in self.indices:
            self.slicer.set_spin_max(idx, self.node.shape[x]-1)
            idx = idx + 1
        
        self.grid.ResetView()

    def get_selected_data(self):
        """
        function to get the selected data in an array
        returns (data, names, line)
            data: array of sliced data
            names: name array for plots
            line: bool-value, True if 1D-Line, False if 2D
        """
        cols = self.grid.GetSelectedCols()
        rows = self.grid.GetSelectedRows()
        rank = len(self.node.shape)
        
        # Scalar data can't be line-plotted.
        if rank == 0:
            return None, None, True

        # Get data currently in the grid
        has_field_names = self.node.dtype is not None and self.node.dtype.names is not None
        if rank > 1 and not has_field_names:
            args = []
            for x in range(rank):
                if x == self.row:
                    args.append(slice(None, None, None))
                elif x == self.col:
                    args.append(slice(None, None, None))
                else:
                    idx = 0
                    for y in self.indices:
                        if y == x:
                            args.append(self.slicer.indices[idx])
                            break
                        idx = idx + 1
            data = self.node[tuple(args)]
            if self.row > self.col:
                data = np.transpose(data)
        else:
            # For structured arrays, use slice access to maintain field name support
            if has_field_names and self.slicer.indices == ():
                data = self.node[:]
            else:
                data = self.node[self.slicer.indices]

        # Columns in the view are selected
        if len(cols) != 0:
            # The data is compound
            if has_field_names:
                names = [self.grid.GetColLabelValue(x) for x in cols]
                data = [data[n] for n in names]
                return data, names, True

            # Plot multiple columns independently
            else:
                if rank > 1:
                    data = [data[(slice(None, None, None),c)] for c in cols]

                names = ["Col %d" % c for c in cols] if len(data) > 1 else None
                return data, names, True

        # Rows in view are selected
        elif len(rows) != 0:
            
            data = [data[(r,)] for r in rows]
            names = ["Row %d" % r for r in rows] if len(data) > 1 else None
            return data, names, True
        
        # No row or column selection.  Plot everything
        else:
            # The data is compound
            if has_field_names:
                names = [self.grid.GetColLabelValue(x) for x in range(self.grid.GetNumberCols())]
                data = [data[n] for n in names]
                return data, names, True

            # Plot 1D
            elif rank == 1:
                return [data], [], True

            # Plot 2D
            else:
                return data, [], False

    def on_sliced(self, evt):
        """ User has chosen to display a different part of the dataset. """
        self.grid.Refresh()

    def on_workaround_timer(self, evt):
        """ See slicer.enable_spinctrls docs """
        self.timer.Destroy()
        self.slicer.enable_spinctrls()

    @property
    def indices(self):
        """ A tuple of integer indices appropriate for dim selection.

        """
        l = []
        for x in range(len(self.node.shape)):
            if x == self.row or x == self.col:
                continue    
            l.append(x)
        return tuple(l)
        
    @property
    def row(self):
        """ The dimension selected for the row
        """
        if self.rowSpin is not None:
            return self.rowSpin.GetValue()
        return 0  # Default to first dimension

    @property
    def col(self):
        """ The dimension selected for the column
        """
        if self.colSpin is not None:
            return self.colSpin.GetValue()
        return 1 if len(self.node.shape) > 1 else 0  # Default to second dimension or first if 1D
        
        
    def on_dimSpin(self, evt):
        """ Dimension Spinbox value changed; notify parent to refresh the grid. """
        if self.rowSpin is None or self.colSpin is None:
            return

        pos = evt.GetPosition()
        otherSpinner = self.rowSpin

        if evt.GetEventObject() == self.rowSpin:
            otherSpinner = self.colSpin

        if pos == otherSpinner.GetValue():
            if (pos > 0):
                pos = pos - 1
            else:
                pos = pos + 1
            otherSpinner.SetValue(pos)

        wx.PostEvent(self, ArraySelectionEvent(self.GetId()))

class SlicerPanel(wx.Panel):
    """
    Holds controls for data access.

    Consult the "indices" property, which returns a tuple of indices that
    prefix the array.  This will be RANK-2 elements long, unless hasfields
    is true, in which case it will be RANK-1 elements long.
    """

    @property
    def indices(self):
        """ A tuple of integer indices appropriate for slicing.

        Will be RANK-2 elements long, RANK-1 if compound data is in use
        (hasfields == True).
        """
        return tuple([x.GetValue() for x in self.spincontrols])

    def __init__(self, parent, shape, hasfields):
        """ Create a new slicer panel.

        parent:     The wxPython parent window
        shape:      Shape of the data to visualize
        hasfields:  If True, the data is compound and the grid can only
                    display one axis.  So, we should display an extra spinbox.
        """
        wx.Panel.__init__(self, parent)
        self.parent = parent
        self.shape = shape
        self.hasfields = hasfields
        self.spincontrols = []

        # Rank of the underlying array
        rank = len(shape)

        # Rank displayable in the grid.  If fields are present, they occupy
        # the columns, so the data displayed is actually 1-D.
        visible_rank = 1 if hasfields else 2

        sizer = wx.BoxSizer(wx.HORIZONTAL)  # Will arrange the SpinCtrls
                
        if rank > visible_rank:
            infotext = wx.StaticText(self, wx.ID_ANY, "Array Indexing: ")
            sizer.Add(infotext, 0, flag=wx.EXPAND | wx.ALL, border=10)

            for idx in range(rank - visible_rank):
                maxVal = shape[idx] - 1
                if not hasfields:
                    maxVal = shape[self.parent.indices[idx]] - 1
                sc = wx.SpinCtrl(self, max=maxVal, value="0", min=0)
                sizer.Add(sc, 0, flag=wx.EXPAND | wx.ALL, border=10)
                sc.Disable()
                self.spincontrols.append(sc)

        self.SetSizer(sizer)

        self.Bind(wx.EVT_SPINCTRL, self.on_spin)

    def enable_spinctrls(self):
        """ Unlock the spin controls.

        Because of a bug in wxPython on Mac, by default the first spin control
        has bizarre contents (and control focus) when the panel starts up.
        Call this after a short delay (e.g. 100 ms) to enable indexing.
        """
        for sc in self.spincontrols:
            sc.Enable()

    def set_spin_max(self, idx, max):
        self.spincontrols[idx].SetRange(0, max)
        
    def on_spin(self, evt):
        """ Spinbox value changed; notify parent to refresh the grid. """
        wx.PostEvent(self, ArraySlicedEvent(self.GetId()))


class ArrayGrid(wx.grid.Grid):
    """
    Grid class to display the Array.

    Cell contents and appearance are handled by the table model in ArrayTable.
    """

    def __init__(self, parent, node, slicer, array_frame=None):
        wx.grid.Grid.__init__(self, parent)
        # Use array_frame if provided, otherwise use parent (for backward compatibility)
        table_parent = array_frame if array_frame is not None else parent
        table = ArrayTable(table_parent)
        self.SetTable(table, True)

        # Set selection mode - try different wxPython versions
        has_field_names = node.dtype is not None and node.dtype.names is not None
        try:
            # Modern wxPython (4.x)
            selmode = wx.grid.Grid.GridSelectionModes.GridSelectColumns
            if not has_field_names and len(node.shape) > 1:
                selmode = wx.grid.Grid.GridSelectionModes.GridSelectRowsAndColumns
        except AttributeError:
            try:
                # Older wxPython versions
                selmode = wx.grid.Grid.wxGridSelectColumns
                if not has_field_names and len(node.shape) > 1:
                    selmode |= wx.grid.Grid.wxGridSelectRows
            except AttributeError:
                try:
                    # Try without wx prefix
                    selmode = wx.grid.GridSelectColumns
                    if not has_field_names and len(node.shape) > 1:
                        selmode |= wx.grid.GridSelectRows
                except AttributeError:
                    # Last resort - use numeric constants
                    selmode = 2  # GridSelectColumns
                    if not has_field_names and len(node.shape) > 1:
                        selmode |= 1  # GridSelectRows

        try:
            self.SetSelectionMode(selmode)
        except Exception:
            # If setting selection mode fails, just continue without it
            pass
           
    def ResetView(self):
            """Trim/extend the grid if needed"""
            rowChange = self.GetTable().GetRowsCount() - self.GetNumberRows()
            colChange = self.GetTable().GetColsCount() - self.GetNumberCols()
            if rowChange != 0 or colChange != 0:
                self.ClearGrid()
                locker = wx.grid.GridUpdateLocker(self)
                if rowChange > 0:
                    msg = wx.grid.GridTableMessage(
                        self.GetTable(),
                        wx.grid.GRIDTABLE_NOTIFY_ROWS_APPENDED,
                        rowChange
                    )
                    self.ProcessTableMessage(msg)
                elif rowChange < 0:
                    msg = wx.grid.GridTableMessage(
                        self.GetTable(),
                        wx.grid.GRIDTABLE_NOTIFY_ROWS_DELETED,
                        0,
                        -rowChange
                    )
                    self.ProcessTableMessage(msg)
                    
                if colChange > 0:
                    msg = wx.grid.GridTableMessage(
                        self.GetTable(),
                        wx.grid.GRIDTABLE_NOTIFY_COLS_APPENDED,
                        colChange
                    )
                    self.ProcessTableMessage(msg)
                elif colChange < 0:
                    msg = wx.grid.GridTableMessage(
                        self.GetTable(),
                        wx.grid.GRIDTABLE_NOTIFY_COLS_DELETED,
                        0,
                        -colChange
                    )
                    self.ProcessTableMessage(msg)

            # The scroll bars aren't resized (at least on windows)
            # Jiggling the size of the window rescales the scrollbars
            # h,w = self.GetSize()
            # self.SetSize((h+1, w))
            # self.SetSize((h, w))
            self.ForceRefresh()


class LRUTileCache(object):
    """
        Simple tile-based LRU cache which goes between the Grid and
        the Array object.  Caches tiles along the last 1 or 2 dimensions
        of a dataset.

        Access is via __getitem__.  Because this class exists specifically
        to support point-based callbacks for the Grid, arguments may
        only be indices, not slices.
    """

    TILESIZE = 100  # Tiles will have shape (100,) or (100, 100)
    MAXTILES = 50  # Max number of tiles to retain in the cache

    def __init__(self, arr):
        """ *arr* is anything implementing compass_model.Array """
        import collections
        self.cache = collections.OrderedDict()
        self.arr = arr

    def __getitem__(self, args):
        """ Restricted to an index or tuple of indices. """

        if not isinstance(args, tuple):
            args = (args,)

        # Split off the last 1 or 2 dimensions
        coarse_position, fine_position = args[0:-2], args[-2:]

        def clip(x):
            """ Round down to nearest TILESIZE; takes e.g. 181 -> 100 """
            return (x // self.TILESIZE) * self.TILESIZE

        # Tuple with index of tile corner
        tile_key = coarse_position + tuple(clip(x) for x in fine_position)

        # Slice which will be applied to dataset to retrieve tile
        tile_slice = coarse_position + tuple(slice(clip(x), clip(x) + self.TILESIZE) for x in fine_position)

        # Index applied to tile to retrieve the desired data point
        tile_data_index = tuple(x % self.TILESIZE for x in fine_position)

        # Case 1: Add tile to cache, ejecting oldest tile if needed
        if not tile_key in self.cache:

            if len(self.cache) >= self.MAXTILES:
                self.cache.popitem(last=False)

            # Use the array's __getitem__ method to ensure proper structured data handling
            # This is important for CSV/Parquet files that return structured records
            if hasattr(self.arr, '__getitem__'):
                tile = self.arr.__getitem__(tile_slice)
            else:
                tile = self.arr[tile_slice]
            self.cache[tile_key] = tile

        # Case 2: Mark the tile as recently accessed
        else:
            tile = self.cache.pop(tile_key)
            self.cache[tile_key] = tile

        return tile[tile_data_index]


class ArrayTable(wx.grid.PyGridTableBase):
    """
    "Table" class which provides data and metadata for the grid to display.

    The methods defined here define the contents of the table, as well as
    the number of rows, columns and their values.
    """

    def __init__(self, parent):
        """ Create a new Table instance for use with a grid control.

        node:     An compass_model.Array implementation instance.
        slicer:   An instance of SlicerPanel, so we can see what indices the
                  user has requested.
        """
        wx.grid.PyGridTableBase.__init__(self)

        self.node = parent.node
        self.selecter = parent
        self.slicer = parent.slicer

        self.rank = len(self.node.shape)
        self.names = self.node.dtype.names if self.node.dtype is not None else None

        self.cache = LRUTileCache(self.node)

    def GetNumberRows(self):
        """ Callback for number of rows displayed by the grid control """
        if self.rank == 0:
            return 1
        elif self.rank == 1:
            return self.node.shape[0]
        elif self.names is not None:
            return self.node.shape[-1]
        return self.node.shape[self.selecter.row]

    def GetNumberCols(self):
        """ Callback for number of columns displayed by the grid control.

        Note that if compound data is in use, columns display the field names.
        """
        if self.names is not None:
            return len(self.names)
        if self.rank < 2:
            return 1
        return self.node.shape[self.selecter.col]

    def GetValue(self, row, col):
        """ Callback which provides data to the Grid.

        row, col:   Integers giving row and column position (0-based).
        """
        try:
            # Scalar case
            if self.rank == 0:
                data = self.node[()]
                if self.names is None:
                    return self._format_value(data)
                return self._format_value(data[col])

            # 1D case
            if self.rank == 1:
                data = self.cache[row]
                if self.names is None:
                    return self._format_value(data)
                return self._format_value(data[self.names[col]])

            # ND case.  Watch out for compound mode!
            if self.names is not None:
                args = self.slicer.indices + (row,)
            else:
                l = []
                for x in range(self.rank):
                    if x == self.selecter.row:
                        l.append(row)
                    elif x == self.selecter.col:
                        l.append(col)
                    else:
                        idx = 0
                        for y in self.selecter.indices:
                            if y == x:
                                l.append(self.slicer.indices[idx])
                                break
                            idx = idx + 1
                args = tuple(l)

            data = self.cache[args]
            if self.names is None:
                return self._format_value(data)
            return self._format_value(data[self.names[col]])

        except Exception as e:
            return f"<Error: {e}>"

    def _format_value(self, value):
        """ Format a value for display in the grid """
        try:
            # Handle numpy scalar types
            # Only call .item() for 0-d arrays or arrays with exactly 1 element
            if hasattr(value, 'item') and hasattr(value, 'size'):
                if value.size == 1:
                    value = value.item()

            # Handle bytes
            if isinstance(value, bytes):
                return value.decode('utf-8', errors='replace')

            # Convert to string
            return str(value)
        except Exception as e:
            return f"<Format Error: {e}>"

    def GetRowLabelValue(self, row):
        """ Callback for row labels.

        Row number is used unless the data is scalar.
        """
        if self.rank == 0:
            return "Value"
        return str(row)

    def GetColLabelValue(self, col):
        """ Callback for column labels.

        Column number is used, except for scalar or 1D data, or if we're
        displaying field names in the columns.
        """
        if self.names is not None:
            return self.names[col]
        if self.rank == 0 or self.rank == 1:
            return "Value"
        return str(col)
