import wx
import pytest
from hdf_compass.compass_viewer.viewer import CompassApp
from hdf_compass.compass_viewer.frame import BaseFrame, NodeFrame
from hdf_compass.compass_viewer.events import CompassOpenEvent
from hdf_compass import compass_model

class MockStore(compass_model.Store):
    @staticmethod
    def plugin_name():
        return "Mock"
    
    @staticmethod
    def plugin_description():
        return "Mock store for testing"
    
    def __init__(self):
        self._valid = True
    
    def __contains__(self, key):
        return key == "/"
    
    @property
    def url(self):
        return "mock://test"
    
    @property
    def display_name(self):
        return "Mock Store"
    
    @property
    def root(self):
        return None
    
    @property
    def valid(self):
        return self._valid
    
    def gethandlers(self, key=None):
        if key is None:
            return [MockNode]
        if key == "/":
            return [MockNode]
        raise KeyError(key)

class MockNode(compass_model.Node):
    class_kind = "Mock Node"
    
    icons = {16: "hdf_compass/compass_viewer/icons/folder_16.png",
             64: "hdf_compass/compass_viewer/icons/folder_64.png"}
    
    @staticmethod
    def can_handle(store, key):
        return True
    
    def __init__(self, store, key="/"):
        self._store = store
        self._key = key
    
    @property
    def key(self):
        return self._key
    
    @property
    def store(self):
        return self._store
    
    @property
    def display_name(self):
        return "Mock Node"
    
    @property
    def description(self):
        return "Mock node for testing"

def test_wxpython_version():
    """Test that we're using wxPython3"""
    assert wx.version().startswith('4')

def test_compass_app_creation():
    """Test that CompassApp can be created"""
    app = CompassApp(False)
    assert isinstance(app, wx.App)

def test_base_frame_creation():
    """Test that BaseFrame can be created"""
    app = CompassApp(False)
    frame = BaseFrame()
    assert isinstance(frame, wx.Frame)

def test_node_frame_creation():
    """Test that NodeFrame can be created"""
    app = CompassApp(False)
    store = MockStore()
    node = MockNode(store)
    frame = NodeFrame(node)
    assert isinstance(frame, BaseFrame)

def test_compass_open_event():
    """Test that CompassOpenEvent can be created"""
    store = MockStore()
    node = MockNode(store)
    event = CompassOpenEvent(node)
    assert isinstance(event, wx.PyCommandEvent)
    assert event.GetEventType() == CompassOpenEvent.typeId 