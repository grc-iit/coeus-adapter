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
Defines wx.Frame subclasses which are the foundation of the various windows
displayed by HDFCompass.

Much of the common functionality (e.g. "Open File..." menu item) is implemented
here.
"""
from __future__ import absolute_import, division, print_function, unicode_literals

import logging
import os
from datetime import date

import wx
import wx.richtext as rtc
from wx.lib.pubsub import pub
from wx.adv import AboutDialogInfo, AboutBox

log = logging.getLogger(__name__)

from .info import InfoPanel

ID_OPEN_RESOURCE = wx.ID_ANY
ID_CLOSE_FILE = wx.ID_ANY
ID_PLUGIN_INFO = wx.ID_ANY

MAX_RECENT_FILES = 8

from hdf_compass import compass_model
from hdf_compass.utils import __version__, is_darwin, path2url
from .events import CompassOpenEvent

open_frames = 0  # count the open frames


class BaseFrame(wx.Frame):
    """
    Base class for all frames used in HDF Compass.

    Implements common menus including File and Help, and handles their
    events.

    When implementing a new viewer window, you should instead inherit from
    BaseFrame (below), which adds a left-hand side information panel, and
    participates in the reference counting that automatically shows the
    initial window when all other frames are closed.
    """

    icon_folder = os.path.abspath(os.path.join(os.path.dirname(__file__), 'icons'))
    open_frames = 0  # count the number of frames

    last_open_path = os.getcwd()

    def __init__(self, **kwds):
        """ Create a new base frame """
        log.debug(self.__class__.__name__)
        # Handle pos=None case for wxPython compatibility
        if 'pos' in kwds and kwds['pos'] is None:
            kwds['pos'] = wx.DefaultPosition
        super(BaseFrame, self).__init__(None, **kwds)

        # Set icon
        icon_32 = wx.Icon()
        icon_32.CopyFromBitmap(wx.Bitmap(os.path.join(self.icon_folder, "favicon_32.png"), wx.BITMAP_TYPE_ANY))
        icon_48 = wx.Icon()
        icon_48.CopyFromBitmap(wx.Bitmap(os.path.join(self.icon_folder, "favicon_48.png"), wx.BITMAP_TYPE_ANY))
        
        # Create icon bundle and add icons
        icon_bundle = wx.IconBundle()
        icon_bundle.AddIcon(icon_32)
        icon_bundle.AddIcon(icon_48)
        self.SetIcons(icon_bundle)

        # Create menu bar
        self.menu_bar = wx.MenuBar()
        self.SetMenuBar(self.menu_bar)

        # Create file menu
        file_menu = wx.Menu()
        file_menu.Append(wx.ID_OPEN, "&Open...\tCtrl+O")
        file_menu.Append(wx.ID_SAVE, "&Save\tCtrl+S")
        file_menu.Append(wx.ID_SAVEAS, "Save &As...\tCtrl+Shift+S")
        file_menu.AppendSeparator()
        file_menu.Append(wx.ID_CLOSE, "&Close\tCtrl+W")
        file_menu.Append(wx.ID_EXIT, "E&xit\tAlt+F4")
        self.menu_bar.Append(file_menu, "&File")

        # Create edit menu
        edit_menu = wx.Menu()
        edit_menu.Append(wx.ID_COPY, "&Copy\tCtrl+C")
        edit_menu.Append(wx.ID_PASTE, "&Paste\tCtrl+V")
        self.menu_bar.Append(edit_menu, "&Edit")

        # Create view menu
        view_menu = wx.Menu()
        view_menu.Append(wx.ID_ZOOM_IN, "Zoom &In\tCtrl++")
        view_menu.Append(wx.ID_ZOOM_OUT, "Zoom &Out\tCtrl+-")
        view_menu.Append(wx.ID_ZOOM_FIT, "Zoom &Fit\tCtrl+0")
        self.menu_bar.Append(view_menu, "&View")

        # Create help menu
        help_menu = wx.Menu()
        help_menu.Append(wx.ID_ABOUT, "&About")
        self.menu_bar.Append(help_menu, "&Help")

        # Create toolbar
        self.toolbar = self.CreateToolBar()
        self.toolbar.AddTool(wx.ID_OPEN, "Open", wx.ArtProvider.GetBitmap(wx.ART_FILE_OPEN))
        self.toolbar.AddTool(wx.ID_SAVE, "Save", wx.ArtProvider.GetBitmap(wx.ART_FILE_SAVE))
        self.toolbar.AddTool(wx.ID_COPY, "Copy", wx.ArtProvider.GetBitmap(wx.ART_COPY))
        self.toolbar.AddTool(wx.ID_PASTE, "Paste", wx.ArtProvider.GetBitmap(wx.ART_PASTE))
        self.toolbar.AddTool(wx.ID_ZOOM_IN, "Zoom In", wx.ArtProvider.GetBitmap(wx.ART_PLUS))
        self.toolbar.AddTool(wx.ID_ZOOM_OUT, "Zoom Out", wx.ArtProvider.GetBitmap(wx.ART_MINUS))
        self.toolbar.AddTool(wx.ID_ZOOM_FIT, "Zoom Fit", wx.ArtProvider.GetBitmap(wx.ART_FIND))
        self.toolbar.Realize()

        # Bind events
        self.Bind(wx.EVT_MENU, self.on_open, id=wx.ID_OPEN)
        self.Bind(wx.EVT_MENU, self.on_save, id=wx.ID_SAVE)
        self.Bind(wx.EVT_MENU, self.on_save_as, id=wx.ID_SAVEAS)
        self.Bind(wx.EVT_MENU, self.on_close, id=wx.ID_CLOSE)
        self.Bind(wx.EVT_MENU, self.on_exit, id=wx.ID_EXIT)
        self.Bind(wx.EVT_MENU, self.on_copy, id=wx.ID_COPY)
        self.Bind(wx.EVT_MENU, self.on_paste, id=wx.ID_PASTE)
        self.Bind(wx.EVT_MENU, self.on_zoom_in, id=wx.ID_ZOOM_IN)
        self.Bind(wx.EVT_MENU, self.on_zoom_out, id=wx.ID_ZOOM_OUT)
        self.Bind(wx.EVT_MENU, self.on_zoom_fit, id=wx.ID_ZOOM_FIT)
        self.Bind(wx.EVT_MENU, self.on_about, id=wx.ID_ABOUT)

    def on_open(self, evt):
        """ Handle open menu item """
        with wx.FileDialog(self, "Open file", wildcard="All files (*.*)|*.*",
                          style=wx.FD_OPEN | wx.FD_FILE_MUST_EXIST) as dlg:
            if dlg.ShowModal() == wx.ID_OK:
                path = dlg.GetPath()
                # Convert file path to URL format and open directly
                url = 'file://' + os.path.abspath(path)
                from .viewer import open_store
                open_store(url)

    def on_save(self, evt):
        """ Handle save menu item """
        pass

    def on_save_as(self, evt):
        """ Handle save as menu item """
        pass

    def on_close(self, evt):
        """ Handle close menu item """
        self.Close()

    def on_exit(self, evt):
        """ Handle exit menu item """
        wx.Exit()

    def on_copy(self, evt):
        """ Handle copy menu item """
        pass

    def on_paste(self, evt):
        """ Handle paste menu item """
        pass

    def on_zoom_in(self, evt):
        """ Handle zoom in menu item """
        pass

    def on_zoom_out(self, evt):
        """ Handle zoom out menu item """
        pass

    def on_zoom_fit(self, evt):
        """ Handle zoom fit menu item """
        pass

    def on_about(self, evt):
        """ Handle about menu item """
        wx.MessageBox("HDF Compass Viewer\n\nA viewer for HDF files",
                     "About HDF Compass", wx.OK | wx.ICON_INFORMATION)

    def add_menu(self, menu, title):
        """ Add a menu to the menu bar """
        menubar = self.GetMenuBar()
        if not menubar:
            menubar = wx.MenuBar()
            self.SetMenuBar(menubar)
        menubar.Append(menu, title)


class InitFrame(BaseFrame):
    """ Frame displayed when the application starts up.

    This includes the menu bar provided by TopFrame.  On the Mac, although it
    still exists (to prevent the application from exiting), the frame
    is typically not shown.
    """

    def __init__(self):
        style = wx.DEFAULT_FRAME_STYLE & (~wx.RESIZE_BORDER) & (~wx.MAXIMIZE_BOX)
        title = "HDF Compass"
        super(InitFrame, self).__init__(size=(552, 247), title=title, style=style)

        data = wx.Bitmap(os.path.join(self.icon_folder, "logo.png"), wx.BITMAP_TYPE_ANY)
        bmp = wx.StaticBitmap(self, wx.ID_ANY, data)

        # The init frame isn't visible on Mac, so there shouldn't be an
        # option to close it.  "Quit" does the same thing.
        if is_darwin:
            mb = self.GetMenuBar()
            mu = mb.GetMenu(0)
            mu.Enable(wx.ID_CLOSE, False)
        self.Center()


class NodeFrame(BaseFrame):
    """ Base class for any frame which displays a Node instance.

    Provides a "Close file" menu item and manages open data stores.

    Has three attributes of note:

    .node:  Settable Node instance to display
    .info:  Read-only InfoPanel instance (left-hand sidebar)
    .view:  Settable wx.Panel instance for the right-hand view.

    In order to coordinate file-close events across multiple frames,
    a reference-counting system is used.  When a new frame that uses a store
    is created, that store's reference count (in cls._stores) is incremented.
    When the frame is closed, the store's count is decremented.  

    When the reference count reaches 0 or the "Close File" is selected from the
    menu, the store is closed and a pubsub notification is sent out to all
    other frames.  They check to see if their .node.store's are valid, and
    if not, close themselves.
    """

    # --- Store reference-counting methods ------------------------------------

    _stores = {}

    @classmethod
    def _incref(cls, store):
        """ Record that a client is using the specified store. """
        try:
            cls._stores[store] += 1
        except KeyError:
            cls._stores[store] = 1

    @classmethod
    def _decref(cls, store):
        """ Record that a client is finished using the specified store. """
        try:
            val = cls._stores[store]
            if val == 1:
                cls._close(store)
                del cls._stores[store]
            else:
                cls._stores[store] = val - 1
        except KeyError:
            pass

    @classmethod
    def _close(cls, store):
        """ Manually close the store, and broadcast a pubsub notification. """
        cls._stores.pop(store, None)
        store.close()
        pub.sendMessage('store.close')

    # --- End store reference-counting ----------------------------------------

    @property
    def info(self):
        """ The InfoPanel object used for the left-hand sidebar. """
        return self.__info

    @property
    def node(self):
        """ Node instance displayed by the frame. """
        return self.__node

    @node.setter
    def node(self, newnode):
        self.__node = newnode

    @property
    def view(self):
        """ Right-hand view """
        return self.__view

    @view.setter
    def view(self, window):
        if self.__view is None:
            self.__sizer.Add(window, 1, wx.EXPAND)
        else:
            self.__sizer.Detach(self.__view)
            self.__view.Destroy()
            self.__sizer.Add(window, 1, wx.EXPAND)
        self.__view = window
        self.Layout()

    def __init__(self, node, **kwds):
        """ Constructor.  Keywords are passed on to wx.Frame.

        node:   The compass_model.Node instance to display.
        """

        super(NodeFrame, self).__init__(**kwds)

        # Enable the "Close File" menu entry if it exists
        try:
            menubar = self.GetMenuBar()
            if menubar and menubar.GetMenuCount() > 0:
                fm = menubar.GetMenu(0)
                if fm and fm.FindItem(ID_CLOSE_FILE) != wx.NOT_FOUND:
                    fm.Enable(ID_CLOSE_FILE, True)
        except wx.wxAssertionError:
            # Menu item doesn't exist, ignore
            pass

        # Create the "window" menu to hold "Reopen As" items.
        wm = wx.Menu()

        # Determine a list of handlers which can understand this object.
        # We exclude the default handler, "Unknown", as it can't do anything.
        # See also container/list.py.
        handlers = [x for x in node.store.gethandlers(node.key) if x != compass_model.Unknown]

        # This will map menu IDs -> Node subclass handlers
        self._menu_handlers = {}

        # Note there's guaranteed to be at least one entry: the class
        # being used for the current frame!
        for h in handlers:
            id_ = wx.ID_ANY
            self._menu_handlers[id_] = h
            wm.Append(id_, "Reopen as " + h.class_kind)
            self.Bind(wx.EVT_MENU, self.on_menu_reopen, id=id_)

        self.GetMenuBar().Insert(1, wm, "&Window")

        self.__node = node
        self.__view = None
        self.__info = InfoPanel(self)

        self.__sizer = wx.BoxSizer(wx.HORIZONTAL)
        self.__sizer.Add(self.__info, 0, wx.EXPAND)
        self.SetSizer(self.__sizer)

        self.info.display(node)

        # Modify existing File menu to add node-specific items
        menubar = self.GetMenuBar()
        if menubar and menubar.GetMenuCount() > 0:
            file_menu = menubar.GetMenu(0)  # File menu is typically the first menu
            if file_menu:
                # Add separator and node-specific menu items
                file_menu.AppendSeparator()
                file_menu.Append(ID_CLOSE_FILE, "Close &File\tCtrl+Shift+W")
        else:
            # Fallback: create File menu if none exists
            file_menu = wx.Menu()
            file_menu.Append(wx.ID_CLOSE, "&Close Window\tCtrl+W")
            file_menu.Append(ID_CLOSE_FILE, "Close &File\tCtrl+Shift+W")
            file_menu.AppendSeparator()
            file_menu.Append(wx.ID_EXIT, "E&xit\tAlt+F4")
            self.add_menu(file_menu, "&File")

        self.Bind(wx.EVT_CLOSE, self.on_close_evt)
        self.Bind(wx.EVT_MENU, self.on_close_window, id=wx.ID_CLOSE)
        self.Bind(wx.EVT_MENU, self.on_menu_closefile, id=ID_CLOSE_FILE)
        self.Bind(wx.EVT_MENU, self.on_exit, id=wx.ID_EXIT)

        self._incref(node.store)
        pub.subscribe(self.on_notification_closefile, 'store.close')

    def on_notification_closefile(self):
        """ Pubsub notification that a file (any file) has been closed """
        try:
            # Check if frame still exists and store is invalid
            if hasattr(self, 'node') and hasattr(self.node, 'store') and not self.node.store.valid:
                # Check if the frame hasn't been destroyed already
                if not self.IsBeingDeleted():
                    self.Destroy()
        except RuntimeError as e:
            # Frame was already destroyed
            log.debug(f"Frame already destroyed: {e}")
        except Exception as e:
            log.exception(f"Error in on_notification_closefile: {e}")

    def on_close_evt(self, evt):
        """ Window is about to be closed """
        self._decref(self.node.store)
        evt.Skip()

    def on_close_window(self, evt):
        """ Close Window menu item activated - close just this window """
        self.Close()

    def on_exit(self, evt):
        """ Exit menu item activated - exit application """
        wx.Exit()

    def on_menu_closefile(self, evt):
        """ "Close File" menu item activated.

        Note we rely on the pubsub message (above) to actually close the frame.
        """
        self._close(self.node.store)

    def on_menu_reopen(self, evt):
        """ Called when one of the "Reopen As" menu items is clicked """

        # The "Reopen As" submenu ID
        id_ = evt.GetId()

        # Present node
        node_being_opened = self.node

        # The requested Node subclass to instantiate.
        h = self._menu_handlers[id_]

        log.debug('opening: %s %s' % (node_being_opened.store, node_being_opened.key))
        # Brand new Node instance of the requested type
        node_new = h(node_being_opened.store, node_being_opened.key)

        # Send off a request for it to be opened in the appropriate viewer
        # Post it directly to the App, or Container will intercept it!
        pos = wx.GetTopLevelParent(self).GetPosition()
        wx.PostEvent(wx.GetApp(), CompassOpenEvent(node_new, pos=pos))


class PluginInfoFrame(wx.Frame):
    icon_folder = os.path.abspath(os.path.join(os.path.dirname(__file__), 'icons'))

    def __init__(self, parent):
        # make that the plugin info is displayed in the middle of the screen
        frame_w = 320
        frame_h = 250
        x = wx.SystemSettings.GetMetric(wx.SYS_SCREEN_X) // 2 - frame_w // 2
        y = wx.SystemSettings.GetMetric(wx.SYS_SCREEN_Y) // 2 - frame_h // 2
        wx.Frame.__init__(self, parent, title="Plugin Info", pos=(x, y), size=(frame_w, frame_h))

        # Frame icon
        ib = wx.IconBundle()
        icon_32 = wx.Icon()
        icon_32.CopyFromBitmap(wx.Bitmap(os.path.join(self.icon_folder, "favicon_32.png"), wx.BITMAP_TYPE_ANY))
        ib.AddIcon(icon_32)
        icon_48 = wx.Icon()
        icon_48.CopyFromBitmap(wx.Bitmap(os.path.join(self.icon_folder, "favicon_48.png"), wx.BITMAP_TYPE_ANY))
        ib.AddIcon(icon_48)
        self.SetIcons(ib)

        p = wx.Panel(self)
        nb = wx.Notebook(p)

        for store in compass_model.get_stores():
            try:
                # log.debug(store.plugin_name())
                # log.debug(store.plugin_description())

                pnl = wx.Panel(nb)
                t = rtc.RichTextCtrl(pnl, -1, style=wx.TE_READONLY)
                t.BeginFontSize(9)
                t.BeginAlignment(wx.TEXT_ALIGNMENT_CENTRE)
                t.BeginBold()
                t.WriteText("Name: ")
                t.EndBold()
                t.BeginItalic()
                t.WriteText(store.plugin_name())
                t.EndItalic()
                t.Newline()
                t.Newline()
                t.BeginBold()
                t.WriteText("Description")
                t.EndBold()
                t.Newline()
                t.BeginItalic()
                t.WriteText(store.plugin_description())
                t.EndItalic()
                t.Newline()

                # store.plugin_description(), style=wx.TE_MULTILINE|wx.TE_READONLY|wx.TE_CENTER)
                szr = wx.BoxSizer()
                szr.Add(t, 1, wx.ALL | wx.EXPAND, 5)
                pnl.SetSizer(szr)
                nb.AddPage(pnl, store.plugin_name())

            except NotImplementedError:
                # skip not implemented plugin name/description
                log.debug("Not implemented name/description for %s" % store)

        sizer = wx.BoxSizer()
        sizer.Add(nb, 1, wx.ALL | wx.EXPAND, 3)
        p.SetSizer(sizer)
