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
Implements a viewer for key-value stores (instances of compass_model.KeyValue).

Keys are strings, values are any data type HDFCompass can understand.
Presently this means all NumPy types, plus Python str/unicode.
"""
from __future__ import absolute_import, division, print_function, unicode_literals

import wx

import logging
log = logging.getLogger(__name__)

from ..frame import NodeFrame


class KeyValueFrame(NodeFrame):

    """
    A frame to display a list of key/value pairs, their types and shapes.
    """

    def __init__(self, node, title=None, size=(800, 400), pos=None, **kwds):
        """ Create a new key-value frame """
        if title is None:
            title = node.display_name
        super(KeyValueFrame, self).__init__(node, size=size, title=title, pos=pos, **kwds)

        # Create list control
        self.list = KeyValueList(self, node)

        # Create sizer
        sizer = wx.BoxSizer(wx.VERTICAL)
        sizer.Add(self.list, 1, wx.EXPAND | wx.ALL, 5)
        self.SetSizer(sizer)

        # Create toolbar
        self.toolbar = self.CreateToolBar()
        self.toolbar.AddTool(wx.ID_COPY, "Copy", wx.ArtProvider.GetBitmap(wx.ART_COPY, wx.ART_TOOLBAR))
        self.toolbar.AddTool(wx.ID_SAVEAS, "Save As", wx.ArtProvider.GetBitmap(wx.ART_FILE_SAVE_AS, wx.ART_TOOLBAR))
        self.toolbar.Realize()

        # Bind events
        self.Bind(wx.EVT_MENU, self.on_copy, id=wx.ID_COPY)
        self.Bind(wx.EVT_MENU, self.on_save_as, id=wx.ID_SAVEAS)

    def on_copy(self, evt):
        """ Handle copy menu item """
        self.list.copy_to_clipboard()

    def on_save_as(self, evt):
        """ Handle save as menu item """
        with wx.FileDialog(
            self,
            "Save key-value pairs",
            wildcard="CSV files (*.csv)|*.csv|Text files (*.txt)|*.txt",
            style=wx.FD_SAVE | wx.FD_OVERWRITE_PROMPT
        ) as dlg:
            if dlg.ShowModal() == wx.ID_OK:
                self.list.save_to_file(dlg.GetPath())


class KeyValueList(wx.ListCtrl):

    """
    A simple list view of key/value attributes
    """

    def __init__(self, parent, node):
        """ Create a new attribute list view.

        parent: wxPython parent object
        node:   compass_model.KeyValue instance
        """

        wx.ListCtrl.__init__(self, parent, style=wx.LC_REPORT | wx.LC_SINGLE_SEL | wx.BORDER_NONE | wx.LC_HRULES)

        self.node = node

        self.InsertColumn(0, "Name")
        self.InsertColumn(1, "Value")
        self.InsertColumn(2, "Type")
        self.InsertColumn(3, "Shape")

        names = node.keys
        values = [self.node[n] for n in names]

        def itemtext(item, col_id):
            name = names[item]
            data = values[item]
            text = ""
            if col_id == 0:
                text = name
            elif col_id == 1:
                text = str(data)
            elif col_id == 2:
                if hasattr(data, 'dtype'):
                    text = str(data.dtype)
                else:
                    text = str(type(data))
            elif col_id == 3:
                if hasattr(data, 'shape'):
                    text = str(data.shape)
                else:
                    text = "()"
            return text

        for n in names:
            row = self.InsertStringItem(9999, n)
            for col in xrange(1, 4):
                self.SetStringItem(row, col, itemtext(row, col))

        self.SetColumnWidth(0, 200)
        self.SetColumnWidth(1, wx.LIST_AUTOSIZE)
        self.SetColumnWidth(2, wx.LIST_AUTOSIZE)
        self.SetColumnWidth(3, wx.LIST_AUTOSIZE)
