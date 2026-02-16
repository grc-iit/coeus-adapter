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
Defines a small number of custom events, which are useful for the GUI.
"""
from __future__ import absolute_import, division, print_function, unicode_literals

import wx
from wx.lib.newevent import NewEvent

import logging
log = logging.getLogger(__name__)

ID_COMPASS_OPEN = wx.ID_ANY

# Create new event type
_CompassOpenEvent, EVT_COMPASS_OPEN = NewEvent()

class CompassOpenEvent(_CompassOpenEvent):
    """ Event sent when a node should be opened """
    def __init__(self, node, pos=None):
        super(CompassOpenEvent, self).__init__()
        self.node = node
        self.pos = pos

    def get_node(self):
        """ Get the node to open """
        return self.node

    """
    Event posted when a request has been made to "open" a object in
    the container.  The source should always be ID_COMPASS_OPEN.

    The type of event is EVT_MENU, because wxPython doesn't like it if we
    use anything else.  When binding handlers, make sure to explicitly
    specify the source ID (or check it in the callback) to avoid catching
    these events by mistake.
    """

# Hints that the selected item in the view may have changed.
# Interested parties should inspect the view to figure out the new selection.
_ContainerSelectionEvent, EVT_CONTAINER_SELECTION = NewEvent()

class ContainerSelectionEvent(_ContainerSelectionEvent):
    """ Event sent when container selection changes """
    def __init__(self, node=None):
        super(ContainerSelectionEvent, self).__init__()
        self.node = node
