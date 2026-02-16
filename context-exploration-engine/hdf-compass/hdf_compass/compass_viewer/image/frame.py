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
Implements a simple true-color image viewer.
"""
from __future__ import absolute_import, division, print_function, unicode_literals

import wx
import wx.grid

import logging
log = logging.getLogger(__name__)

from ..frame import NodeFrame


class ImageFrame(NodeFrame):

    """
        Top-level frame displaying objects of type compass_model.Image.
    """

    def __init__(self, node, title=None, size=(800, 600), **kwds):
        """ Create a new image frame """
        if title is None:
            title = node.display_name
        super(ImageFrame, self).__init__(node, title=title, size=size, **kwds)

        # Create image panel
        self.panel = ImagePanel(self, node)

        # Create sizer
        sizer = wx.BoxSizer(wx.VERTICAL)
        sizer.Add(self.panel, 1, wx.EXPAND | wx.ALL, 5)
        self.SetSizer(sizer)

        # Create toolbar
        self.toolbar = self.CreateToolBar()
        self.toolbar.AddTool(wx.ID_COPY, "Copy", wx.ArtProvider.GetBitmap(wx.ART_COPY, wx.ART_TOOLBAR))
        self.toolbar.AddTool(wx.ID_SAVEAS, "Save As", wx.ArtProvider.GetBitmap(wx.ART_FILE_SAVE_AS, wx.ART_TOOLBAR))
        self.toolbar.AddTool(wx.ID_ZOOM_IN, "Zoom In", wx.ArtProvider.GetBitmap(wx.ART_PLUS, wx.ART_TOOLBAR))
        self.toolbar.AddTool(wx.ID_ZOOM_OUT, "Zoom Out", wx.ArtProvider.GetBitmap(wx.ART_MINUS, wx.ART_TOOLBAR))
        self.toolbar.AddTool(wx.ID_ZOOM_FIT, "Fit to Window", wx.ArtProvider.GetBitmap(wx.ART_FIND, wx.ART_TOOLBAR))
        self.toolbar.Realize()

        # Bind events
        self.Bind(wx.EVT_MENU, self.on_copy, id=wx.ID_COPY)
        self.Bind(wx.EVT_MENU, self.on_save_as, id=wx.ID_SAVEAS)
        self.Bind(wx.EVT_MENU, self.on_zoom_in, id=wx.ID_ZOOM_IN)
        self.Bind(wx.EVT_MENU, self.on_zoom_out, id=wx.ID_ZOOM_OUT)
        self.Bind(wx.EVT_MENU, self.on_zoom_fit, id=wx.ID_ZOOM_FIT)

    def on_copy(self, evt):
        """ Handle copy menu item """
        self.panel.copy_to_clipboard()

    def on_save_as(self, evt):
        """ Handle save as menu item """
        with wx.FileDialog(
            self,
            "Save image file",
            wildcard="PNG files (*.png)|*.png|JPEG files (*.jpg)|*.jpg",
            style=wx.FD_SAVE | wx.FD_OVERWRITE_PROMPT
        ) as dlg:
            if dlg.ShowModal() == wx.ID_OK:
                self.panel.save_image(dlg.GetPath())

    def on_zoom_in(self, evt):
        """ Handle zoom in menu item """
        self.panel.zoom_in()

    def on_zoom_out(self, evt):
        """ Handle zoom out menu item """
        self.panel.zoom_out()

    def on_zoom_fit(self, evt):
        """ Handle zoom fit menu item """
        self.panel.zoom_fit()


class ImagePanel(wx.Panel):

    """
    Panel inside the image viewer pane which displays the image.
    """

    def __init__(self, parent, node):
        """ Display a true color, pixel-interlaced (not pixel-planar) image
        """
        wx.Panel.__init__(self, parent)
        b = wx.BitmapFromBuffer(node.width, node.height, node.data)
        b.CopyFromBuffer(node.data)

        sizer = wx.BoxSizer(wx.HORIZONTAL)

        sb = wx.StaticBitmap(self, wx.ID_ANY, b)
        sizer.AddStretchSpacer()
        sizer.Add(sb, 1, wx.EXPAND | wx.ALIGN_CENTER | wx.ALIGN_CENTER_VERTICAL)
        sizer.AddStretchSpacer()

        sizer2 = wx.BoxSizer(wx.VERTICAL)
        sizer2.AddStretchSpacer()
        sizer2.Add(sizer, 1)
        sizer2.AddStretchSpacer()

        self.SetSizer(sizer2)
