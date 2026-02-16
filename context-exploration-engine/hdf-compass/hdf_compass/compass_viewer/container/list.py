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
Handles list and icon view for Container display.
"""
from __future__ import absolute_import, division, print_function, unicode_literals

import wx
from wx.lib.pubsub import pub

import logging
log = logging.getLogger(__name__)

from hdf_compass import compass_model
from ..events import CompassOpenEvent
from ..events import ContainerSelectionEvent

ID_CONTEXT_MENU_OPEN = wx.ID_ANY
ID_CONTEXT_MENU_OPENWINDOW = wx.ID_ANY
ID_CONTEXT_MENU_COPY = wx.ID_ANY


class ContainerList(wx.ListCtrl):
    """ List control for displaying container contents """

    def __init__(self, parent, node, **kwds):
        """ Create a new container list control """
        default_style = wx.LC_REPORT | wx.LC_SINGLE_SEL | wx.BORDER_SUNKEN
        if 'style' in kwds:
            # Use the passed style, preserving non-conflicting flags
            passed_style = kwds.pop('style')
            # Keep BORDER and SINGLE_SEL but replace the view mode
            style = passed_style | wx.LC_SINGLE_SEL | wx.BORDER_SUNKEN
        else:
            style = default_style
        super(ContainerList, self).__init__(parent, style=style, **kwds)

        self.node = node
        self.parent = parent

        # Create columns only for report view
        if style & wx.LC_REPORT:
            self.InsertColumn(0, "Name")
            self.InsertColumn(1, "Type")
            self.InsertColumn(2, "Size")

            # Set column widths
            self.SetColumnWidth(0, 200)
            self.SetColumnWidth(1, 100)
            self.SetColumnWidth(2, 100)

        # Set size hints
        self.SetMinSize((400, 300))

        # Populate list
        self.populate()

        # Bind events
        self.Bind(wx.EVT_LIST_ITEM_ACTIVATED, self.on_item_activated)
        self.Bind(wx.EVT_LIST_ITEM_RIGHT_CLICK, self.on_right_click)
        self.Bind(wx.EVT_MENU, self.on_copy, id=wx.ID_COPY)
        self.Bind(wx.EVT_LIST_ITEM_SELECTED, self.on_item_selected)

    def populate(self):
        """ Populate the list with container contents """
        self.DeleteAllItems()
        for i, child in enumerate(self.node):
            self.InsertItem(i, child.display_name)
            self.SetItem(i, 1, child.__class__.__name__)
            self.SetItem(i, 2, str(child.size))

    def on_item_activated(self, evt):
        """ Handle item activation """
        index = evt.GetIndex()
        try:
            # Check if store is still valid (not closed during shutdown)
            if not hasattr(self.node, 'store') or not self.node.store.valid:
                log.debug("Store is no longer valid, ignoring item activation")
                evt.Skip()
                return
                
            if index < 0 or index >= len(self.node):
                log.warning(f"Item activation index {index} out of bounds")
                evt.Skip()
                return
            child = self.node[index]
            # Get the current position of the parent frame
            pos = self.parent.GetPosition()
            # Create and post the open event with position
            open_event = CompassOpenEvent(child, pos=pos)
            wx.PostEvent(wx.GetApp(), open_event)
        except Exception as e:
            log.exception(f"Error in item activation: {e}")
        evt.Skip()

    def on_item_selected(self, evt):
        """ Handle item selection """
        index = evt.GetIndex()
        try:
            # Check if store is still valid (not closed during shutdown)
            if not hasattr(self.node, 'store') or not self.node.store.valid:
                log.debug("Store is no longer valid, ignoring item selection")
                evt.Skip()
                return
                
            if index < 0 or index >= len(self.node):
                log.warning(f"Item selection index {index} out of bounds")
                evt.Skip()
                return
            child = self.node[index]
            # Create and post the selection event
            selection_event = ContainerSelectionEvent(child)
            wx.PostEvent(self.parent, selection_event)
        except Exception as e:
            log.exception(f"Error in item selection: {e}")
        evt.Skip()

    def on_right_click(self, evt):
        """ Handle right click """
        menu = wx.Menu()
        menu.Append(wx.ID_COPY, "Copy")
        self.PopupMenu(menu)
        menu.Destroy()

    def on_copy(self, evt):
        """ Handle copy menu item """
        index = self.GetFirstSelected()
        if index != -1:
            child = self.node[index]
            pub.sendMessage('compass.copy', node=child)

    def copy_to_clipboard(self):
        """ Copy selected item to clipboard """
        index = self.GetFirstSelected()
        if index != -1:
            child = self.node[index]
            pub.sendMessage('compass.copy', node=child)

    @property
    def selection(self):
        """ Return the currently selected node, or None """
        try:
            # Check if store is still valid (not closed during shutdown)
            if not hasattr(self.node, 'store') or not self.node.store.valid:
                log.debug("Store is no longer valid, returning None for selection")
                return None
                
            index = self.GetFirstSelected()
            if index != -1:
                return self.node[index]
            return None
        except Exception as e:
            log.debug(f"Error getting selection (likely due to shutdown): {e}")
            return None


class ContainerIconList(ContainerList):
    """
    Icon view of nodes in a Container.
    """

    def __init__(self, parent, node):
        """ New icon list view
        """
        style = wx.LC_ICON | wx.LC_AUTOARRANGE | wx.BORDER_SUNKEN
        super(ContainerIconList, self).__init__(parent, node, style=style)
        
        self.node = node

        # Set minimum size for icon view
        self.SetMinSize((600, 400))

        # Set up image list for icons
        app = wx.GetApp()
        if not hasattr(app, 'imagelists') or 64 not in app.imagelists:
            # Ensure imagelists are initialized
            app.init_imagelists()
        self.il = app.imagelists[64]
        self.SetImageList(self.il, wx.IMAGE_LIST_NORMAL)

        # Clear any existing items
        self.DeleteAllItems()

        # Populate with icons
        for item in range(len(self.node)):
            try:
                subnode = self.node[item]
                if hasattr(self, 'il') and self.il is not None:
                    image_index = self.il.get_index(type(subnode))
                    self.InsertItem(item, subnode.display_name, image_index)
                else:
                    # Fallback without icon if image list not available
                    self.InsertItem(item, subnode.display_name)
            except:
                log.exception("Error adding icon item")

        # Force initial layout
        self.Layout()
        self.Refresh()

    def populate(self):
        """ Override populate to handle icon view """
        # Ensure image list is available
        if not hasattr(self, 'il') or self.il is None:
            app = wx.GetApp()
            if not hasattr(app, 'imagelists') or 64 not in app.imagelists:
                app.init_imagelists()
            self.il = app.imagelists[64]
            self.SetImageList(self.il, wx.IMAGE_LIST_NORMAL)
        
        self.DeleteAllItems()
        for item in range(len(self.node)):
            try:
                subnode = self.node[item]
                if hasattr(self, 'il') and self.il is not None:
                    image_index = self.il.get_index(type(subnode))
                    self.InsertItem(item, subnode.display_name, image_index)
                else:
                    # Fallback without icon if image list not available
                    self.InsertItem(item, subnode.display_name)
            except:
                log.exception("Error adding icon item")
        self.Layout()
        self.Refresh()


class ContainerReportList(ContainerList):
    """
    List view of the container's contents.

    Uses a wxPython virtual list, allowing millions of items in a container
    without any slowdowns.
    """

    def __init__(self, parent, node):
        """ Create a new list view.

        parent: wxPython parent object
        node:   Container instance to be displayed
        """

        style = wx.LC_REPORT | wx.LC_VIRTUAL | wx.LC_SINGLE_SEL | wx.BORDER_SUNKEN
        super(ContainerReportList, self).__init__(parent, node, style=style)

        self.node = node

        # Clear and recreate columns for report view
        self.ClearAll()
        self.InsertColumn(0, "Name")
        self.InsertColumn(1, "Kind")
        self.SetColumnWidth(0, 300)
        self.SetColumnWidth(1, 200)

        # Set minimum size
        self.SetMinSize((500, 400))

        # Set up image list
        app = wx.GetApp()
        if not hasattr(app, 'imagelists') or 16 not in app.imagelists:
            # Ensure imagelists are initialized
            app.init_imagelists()
        self.il = app.imagelists[16]
        self.SetImageList(self.il, wx.IMAGE_LIST_SMALL)

        # Set up virtual list
        self.SetItemCount(len(node))

        # Note: EVT_LIST_ITEM_ACTIVATED is already bound by parent ContainerList class
        # Only bind the selection event which might need different handling for virtual list
        self.Bind(wx.EVT_LIST_ITEM_SELECTED, self.on_item_selected)

        # Force initial layout
        self.Layout()
        self.Refresh()

    def OnGetItemText(self, item, col):
        """ Callback method to support virtual list ctrl """
        try:
            # Check if store is still valid (not closed during shutdown)
            if not hasattr(self.node, 'store') or not self.node.store.valid:
                return ""
                
            # Check bounds first
            if item < 0 or item >= len(self.node):
                log.warning(f"Item index {item} out of bounds (length: {len(self.node)})")
                return ""
                
            subnode = self.node[item]
            if col == 0:
                return subnode.display_name
            elif col == 1:
                return type(subnode).class_kind
        except Exception as e:
            log.debug(f"Error getting item text for item {item}, col {col} (likely due to shutdown): {e}")
        return ""

    def OnGetItemImage(self, item):
        """ Callback method to support virtual list ctrl """
        try:
            # Check if store is still valid (not closed during shutdown)
            if not hasattr(self.node, 'store') or not self.node.store.valid:
                return -1
                
            # Check bounds first
            if item < 0 or item >= len(self.node):
                log.warning(f"Item index {item} out of bounds (length: {len(self.node)})")
                return -1
                
            subnode = self.node[item]
            if hasattr(self, 'il') and self.il is not None:
                return self.il.get_index(type(subnode))
            return -1
        except Exception as e:
            log.debug(f"Error getting item image for item {item} (likely due to shutdown): {e}")
            return -1

    def populate(self):
        """ Override populate to use virtual list """
        try:
            # Check if store is still valid (not closed during shutdown)
            if not hasattr(self.node, 'store') or not self.node.store.valid:
                log.debug("Store is no longer valid, setting item count to 0")
                self.SetItemCount(0)
                return
                
            node_length = len(self.node)
            log.debug(f"Setting virtual list item count to {node_length}")
            self.SetItemCount(node_length)
            self.Refresh()
        except Exception as e:
            log.exception(f"Error in populate: {e}")
            self.SetItemCount(0)
