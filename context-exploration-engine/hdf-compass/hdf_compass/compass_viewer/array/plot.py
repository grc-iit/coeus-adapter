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
Matplotlib window with toolbar.
"""
from __future__ import absolute_import, division, print_function, unicode_literals

import numpy as np
import wx

# matplotlib backend is already set by viewer.py to WXAgg
import matplotlib
import matplotlib.pyplot as plt
from matplotlib.figure import Figure
from matplotlib.backends.backend_wxagg import FigureCanvasWxAgg as FigCanvas
from matplotlib.backends.backend_wx import NavigationToolbar2Wx as NavigationToolbar

import logging
log = logging.getLogger(__name__)

from ..frame import BaseFrame

ID_VIEW_CMAP_JET = wx.ID_ANY  # default
ID_VIEW_CMAP_BONE = wx.ID_ANY
ID_VIEW_CMAP_GIST_EARTH = wx.ID_ANY
ID_VIEW_CMAP_OCEAN = wx.ID_ANY
ID_VIEW_CMAP_RAINBOW = wx.ID_ANY
ID_VIEW_CMAP_RDYLGN = wx.ID_ANY
ID_VIEW_CMAP_WINTER = wx.ID_ANY
ID_SAVE_AS = wx.ID_ANY
ID_COPY = wx.ID_ANY
ID_ZOOM_IN = wx.ID_ANY
ID_ZOOM_OUT = wx.ID_ANY
ID_ZOOM_FIT = wx.ID_ANY


class PlotFrame(BaseFrame):
    """ Base class for Matplotlib plot windows.

    Override draw_figure() to plot your figure on the provided axes.
    """

    def __init__(self, data, title="a title"):
        """ Create a new Matplotlib plotting window for a 1D line plot """

        log.debug(self.__class__.__name__)
        super(PlotFrame, self).__init__(id=wx.ID_ANY, title=title, size=(800, 400))

        self.data = data

        self.panel = wx.Panel(self)

        self.dpi = 100
        self.fig = Figure((6.0, 4.0), dpi=self.dpi)
        self.canvas = FigCanvas(self.panel, -1, self.fig)

        self.axes = self.fig.add_subplot(111)
        self.axes.set_xlabel('')
        self.axes.set_ylabel('')

        self.toolbar = NavigationToolbar(self.canvas)

        self.vbox = wx.BoxSizer(wx.VERTICAL)
        self.vbox.Add(self.canvas, 1, wx.LEFT | wx.TOP | wx.GROW)
        self.vbox.Add(self.toolbar, 0, wx.EXPAND)

        self.panel.SetSizer(self.vbox)
        self.vbox.Fit(self)

        # Add standard toolbar buttons
        self.toolbar.AddTool(wx.ID_SAVE, "Save", 
                            wx.ArtProvider.GetBitmap(wx.ART_FILE_SAVE, wx.ART_TOOLBAR),
                            "Save plot to file")
        
        self.toolbar.AddTool(wx.ID_COPY, "Copy", 
                            wx.ArtProvider.GetBitmap(wx.ART_COPY, wx.ART_TOOLBAR),
                            "Copy plot to clipboard")
        
        self.toolbar.AddTool(wx.ID_ZOOM_IN, "Zoom In", 
                            wx.ArtProvider.GetBitmap(wx.ART_ADD_BOOKMARK, wx.ART_TOOLBAR),
                            "Zoom in")
        
        self.toolbar.AddTool(wx.ID_ZOOM_OUT, "Zoom Out", 
                            wx.ArtProvider.GetBitmap(wx.ART_DEL_BOOKMARK, wx.ART_TOOLBAR),
                            "Zoom out")
        
        self.toolbar.AddTool(wx.ID_ZOOM_FIT, "Fit", 
                            wx.ArtProvider.GetBitmap(wx.ART_FIND, wx.ART_TOOLBAR),
                            "Fit plot to window")
        
        self.toolbar.Realize()

        # Bind events
        self.Bind(wx.EVT_TOOL, self.on_save, id=wx.ID_SAVE)
        self.Bind(wx.EVT_TOOL, self.on_copy, id=wx.ID_COPY)
        self.Bind(wx.EVT_TOOL, self.on_zoom_in, id=wx.ID_ZOOM_IN)
        self.Bind(wx.EVT_TOOL, self.on_zoom_out, id=wx.ID_ZOOM_OUT)
        self.Bind(wx.EVT_TOOL, self.on_zoom_fit, id=wx.ID_ZOOM_FIT)

        self.draw_figure()

    def draw_figure(self):
        raise NotImplementedError


class LinePlotFrame(PlotFrame):
    def __init__(self, data, names=None, title="Line Plot"):
        self.names = names
        self.complex_mode = 'magnitude'  # Default mode for complex data
        PlotFrame.__init__(self, data, title)

        # Add complex data controls if needed
        if self._has_complex_data():
            self._add_complex_controls()

    def _has_complex_data(self):
        """Check if any of the data arrays contain complex numbers"""
        for d in self.data:
            if np.iscomplexobj(d):
                return True
        return False

    def _add_complex_controls(self):
        """Add menu for complex data visualization options"""
        complex_menu = wx.Menu()

        item_mag = complex_menu.AppendRadioItem(wx.ID_ANY, "Magnitude")
        item_phase = complex_menu.AppendRadioItem(wx.ID_ANY, "Phase")
        item_real = complex_menu.AppendRadioItem(wx.ID_ANY, "Real Part")
        item_imag = complex_menu.AppendRadioItem(wx.ID_ANY, "Imaginary Part")
        item_both = complex_menu.AppendRadioItem(wx.ID_ANY, "Real && Imaginary")

        self.add_menu(complex_menu, "Complex Display")

        # Check magnitude by default
        item_mag.Check(True)

        self.Bind(wx.EVT_MENU, lambda e: self._on_complex_mode('magnitude'), id=item_mag.GetId())
        self.Bind(wx.EVT_MENU, lambda e: self._on_complex_mode('phase'), id=item_phase.GetId())
        self.Bind(wx.EVT_MENU, lambda e: self._on_complex_mode('real'), id=item_real.GetId())
        self.Bind(wx.EVT_MENU, lambda e: self._on_complex_mode('imag'), id=item_imag.GetId())
        self.Bind(wx.EVT_MENU, lambda e: self._on_complex_mode('both'), id=item_both.GetId())

    def _on_complex_mode(self, mode):
        """Handle complex display mode change"""
        self.complex_mode = mode
        self.axes.clear()
        self.draw_figure()
        self.canvas.draw()

    def draw_figure(self):
        lines = []
        plot_names = []

        for idx, d in enumerate(self.data):
            name = self.names[idx] if self.names and idx < len(self.names) else f"Data {idx}"

            if np.iscomplexobj(d):
                # Handle complex data based on mode
                if self.complex_mode == 'magnitude':
                    lines.append(self.axes.plot(np.abs(d), label=f"{name} (magnitude)")[0])
                    plot_names.append(f"{name} (magnitude)")
                elif self.complex_mode == 'phase':
                    lines.append(self.axes.plot(np.angle(d), label=f"{name} (phase)")[0])
                    plot_names.append(f"{name} (phase)")
                    self.axes.set_ylabel('Phase (radians)')
                elif self.complex_mode == 'real':
                    lines.append(self.axes.plot(d.real, label=f"{name} (real)")[0])
                    plot_names.append(f"{name} (real)")
                elif self.complex_mode == 'imag':
                    lines.append(self.axes.plot(d.imag, label=f"{name} (imag)")[0])
                    plot_names.append(f"{name} (imag)")
                elif self.complex_mode == 'both':
                    lines.append(self.axes.plot(d.real, label=f"{name} (real)", linestyle='-')[0])
                    lines.append(self.axes.plot(d.imag, label=f"{name} (imag)", linestyle='--')[0])
                    plot_names.append(f"{name} (real)")
                    plot_names.append(f"{name} (imag)")
            else:
                # Regular real data
                lines.append(self.axes.plot(d, label=name)[0])
                plot_names.append(name)

        if plot_names:
            self.axes.legend()

class LineXYPlotFrame(PlotFrame):
    def __init__(self, data, names=None, title="Line XY Plot"):
        self.names = names
        self.complex_mode = 'magnitude'  # Default mode for complex data
        PlotFrame.__init__(self, data, title)

        # Add complex data controls if needed
        if self._has_complex_data():
            self._add_complex_controls()

    def _has_complex_data(self):
        """Check if any of the data arrays contain complex numbers"""
        for d in self.data:
            if np.iscomplexobj(d):
                return True
        return False

    def _add_complex_controls(self):
        """Add menu for complex data visualization options"""
        complex_menu = wx.Menu()

        item_mag = complex_menu.AppendRadioItem(wx.ID_ANY, "Magnitude")
        item_phase = complex_menu.AppendRadioItem(wx.ID_ANY, "Phase")
        item_real = complex_menu.AppendRadioItem(wx.ID_ANY, "Real Part")
        item_imag = complex_menu.AppendRadioItem(wx.ID_ANY, "Imaginary Part")

        self.add_menu(complex_menu, "Complex Display")

        # Check magnitude by default
        item_mag.Check(True)

        self.Bind(wx.EVT_MENU, lambda e: self._on_complex_mode('magnitude'), id=item_mag.GetId())
        self.Bind(wx.EVT_MENU, lambda e: self._on_complex_mode('phase'), id=item_phase.GetId())
        self.Bind(wx.EVT_MENU, lambda e: self._on_complex_mode('real'), id=item_real.GetId())
        self.Bind(wx.EVT_MENU, lambda e: self._on_complex_mode('imag'), id=item_imag.GetId())

    def _on_complex_mode(self, mode):
        """Handle complex display mode change"""
        self.complex_mode = mode
        self.axes.clear()
        self.draw_figure()
        self.canvas.draw()

    def _process_complex_data(self, d):
        """Convert complex data to real based on current mode"""
        if not np.iscomplexobj(d):
            return d

        if self.complex_mode == 'magnitude':
            return np.abs(d)
        elif self.complex_mode == 'phase':
            return np.angle(d)
        elif self.complex_mode == 'real':
            return d.real
        elif self.complex_mode == 'imag':
            return d.imag
        return np.abs(d)  # Default to magnitude

    def draw_figure(self):
        # Process X data (first column)
        x_data = self._process_complex_data(self.data[0])

        self.axes.set_xlabel(self.names[0] if self.names else "X")
        if len(self.data)==2:
            # a simple X-Y plot using 2 columns
            ylabel = self.names[1] if self.names and len(self.names) > 1 else "Y"
            if np.iscomplexobj(self.data[1]):
                ylabel += f" ({self.complex_mode})"
            self.axes.set_ylabel(ylabel)

        lines = []
        for idx, d in enumerate(self.data[1::], start=1):
            y_data = self._process_complex_data(d)
            label = self.names[idx] if self.names and idx < len(self.names) else f"Y{idx}"
            if np.iscomplexobj(d):
                label += f" ({self.complex_mode})"
            lines.append(self.axes.plot(x_data, y_data, label=label)[0])

        if self.names is not None and len(lines) > 0:
            self.axes.legend()

class ContourPlotFrame(PlotFrame):
    def __init__(self, data, names=None, title="Contour Plot"):
        # need to be set before calling the parent (need for plotting)
        self.colormap = "jet"
        self.cb = None  # matplotlib color-bar
        self.complex_mode = 'magnitude'  # Default mode for complex data

        PlotFrame.__init__(self, data, title)

        # Add complex data controls if needed
        if np.iscomplexobj(self.data):
            self._add_complex_controls()

        self.cmap_menu = wx.Menu()
        self.cmap_menu.Append(ID_VIEW_CMAP_JET, "Jet", kind=wx.ITEM_RADIO)
        self.cmap_menu.Append(ID_VIEW_CMAP_BONE, "Bone", kind=wx.ITEM_RADIO)
        self.cmap_menu.Append(ID_VIEW_CMAP_GIST_EARTH, "Gist Earth", kind=wx.ITEM_RADIO)
        self.cmap_menu.Append(ID_VIEW_CMAP_OCEAN, "Ocean", kind=wx.ITEM_RADIO)
        self.cmap_menu.Append(ID_VIEW_CMAP_RAINBOW, "Rainbow", kind=wx.ITEM_RADIO)
        self.cmap_menu.Append(ID_VIEW_CMAP_RDYLGN, "Red-Yellow-Green", kind=wx.ITEM_RADIO)
        self.cmap_menu.Append(ID_VIEW_CMAP_WINTER, "Winter", kind=wx.ITEM_RADIO)
        self.add_menu(self.cmap_menu, "Colormap")

        self.Bind(wx.EVT_MENU, self.on_cmap_jet, id=ID_VIEW_CMAP_JET)
        self.Bind(wx.EVT_MENU, self.on_cmap_bone, id=ID_VIEW_CMAP_BONE)
        self.Bind(wx.EVT_MENU, self.on_cmap_gist_earth, id=ID_VIEW_CMAP_GIST_EARTH)
        self.Bind(wx.EVT_MENU, self.on_cmap_ocean, id=ID_VIEW_CMAP_OCEAN)
        self.Bind(wx.EVT_MENU, self.on_cmap_rainbow, id=ID_VIEW_CMAP_RAINBOW)
        self.Bind(wx.EVT_MENU, self.on_cmap_rdylgn, id=ID_VIEW_CMAP_RDYLGN)
        self.Bind(wx.EVT_MENU, self.on_cmap_winter, id=ID_VIEW_CMAP_WINTER)

        self.status_bar = wx.StatusBar(self, -1)
        self.status_bar.SetFieldsCount(2)
        self.SetStatusBar(self.status_bar)

        self.canvas.mpl_connect('motion_notify_event', self.update_status_bar)
        self.canvas.Bind(wx.EVT_ENTER_WINDOW, self.change_cursor)

    def on_cmap_jet(self, evt):
        log.debug("cmap: jet")
        self.colormap = "jet"
        self._refresh_plot()

    def on_cmap_bone(self, evt):
        log.debug("cmap: bone")
        self.colormap = "bone"
        self._refresh_plot()

    def on_cmap_gist_earth(self, evt):
        log.debug("cmap: gist_earth")
        self.colormap = "gist_earth"
        self._refresh_plot()

    def on_cmap_ocean(self, evt):
        log.debug("cmap: ocean")
        self.colormap = "ocean"
        self._refresh_plot()

    def on_cmap_rainbow(self, evt):
        log.debug("cmap: rainbow")
        self.colormap = "rainbow"
        self._refresh_plot()

    def on_cmap_rdylgn(self, evt):
        log.debug("cmap: RdYlGn")
        self.colormap = "RdYlGn"
        self._refresh_plot()

    def on_cmap_winter(self, evt):
        log.debug("cmap: winter")
        self.colormap = "winter"
        self._refresh_plot()

    def _add_complex_controls(self):
        """Add menu for complex data visualization options"""
        complex_menu = wx.Menu()

        item_mag = complex_menu.AppendRadioItem(wx.ID_ANY, "Magnitude")
        item_phase = complex_menu.AppendRadioItem(wx.ID_ANY, "Phase")
        item_real = complex_menu.AppendRadioItem(wx.ID_ANY, "Real Part")
        item_imag = complex_menu.AppendRadioItem(wx.ID_ANY, "Imaginary Part")

        self.add_menu(complex_menu, "Complex Display")

        # Check magnitude by default
        item_mag.Check(True)

        self.Bind(wx.EVT_MENU, lambda e: self._on_complex_mode('magnitude'), id=item_mag.GetId())
        self.Bind(wx.EVT_MENU, lambda e: self._on_complex_mode('phase'), id=item_phase.GetId())
        self.Bind(wx.EVT_MENU, lambda e: self._on_complex_mode('real'), id=item_real.GetId())
        self.Bind(wx.EVT_MENU, lambda e: self._on_complex_mode('imag'), id=item_imag.GetId())

    def _on_complex_mode(self, mode):
        """Handle complex display mode change"""
        self.complex_mode = mode
        self.axes.clear()
        self._refresh_plot()

    def _refresh_plot(self):
        self.draw_figure()
        self.canvas.draw()

    def _process_complex_data(self, data):
        """Convert complex data to real based on current mode"""
        if not np.iscomplexobj(data):
            return data

        if self.complex_mode == 'magnitude':
            return np.abs(data)
        elif self.complex_mode == 'phase':
            return np.angle(data)
        elif self.complex_mode == 'real':
            return data.real
        elif self.complex_mode == 'imag':
            return data.imag
        return np.abs(data)  # Default to magnitude

    def draw_figure(self):
        max_elements = 500  # don't attempt plot more than 500x500 elements
        rows = self.data.shape[0]
        cols = self.data.shape[1]
        row_stride = rows // max_elements + 1
        col_stride = cols // max_elements + 1

        # Process complex data if needed
        plot_data = self._process_complex_data(self.data[::row_stride, ::col_stride])

        xx = np.arange(0, self.data.shape[1], col_stride)
        yy = np.arange(0, self.data.shape[0], row_stride)
        img = self.axes.contourf(xx, yy, plot_data, 25, cmap=plt.cm.get_cmap(self.colormap))
        self.axes.set_aspect('equal')
        if self.cb:
            self.cb.on_mappable_changed(img)
        else:
            self.cb = plt.colorbar(img, ax=self.axes)
        self.cb.ax.tick_params(labelsize=8)

        # Update colorbar label if complex data
        if np.iscomplexobj(self.data):
            self.cb.set_label(self.complex_mode.capitalize())

    def change_cursor(self, event):
        self.canvas.SetCursor(wx.StockCursor(wx.CURSOR_CROSS))

    def update_status_bar(self, event):
        msg = str()
        if event.inaxes:
            x, y = int(event.xdata), int(event.ydata)
            if 0 <= y < self.data.shape[0] and 0 <= x < self.data.shape[1]:
                z = self.data[y, x]
                if np.iscomplexobj(self.data):
                    # Show complex value information
                    mag = np.abs(z)
                    phase = np.angle(z)
                    msg = f"x={x}, y={y}, z={z:.3f}, |z|={mag:.3f}, ∠z={phase:.3f} rad"
                else:
                    msg = "x= %d, y= %d, z= %f" % (x, y, z)
        self.status_bar.SetStatusText(msg, 1)
