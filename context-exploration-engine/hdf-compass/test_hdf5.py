import os
import sys
import logging

# Set up logging
logging.basicConfig(level=logging.DEBUG)
log = logging.getLogger(__name__)

# Add the project root to the Python path
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__))))

from hdf_compass.compass_model.test import container, store
from hdf_compass.hdf5_model import HDF5Group, HDF5Store
from hdf_compass.utils import data_url

def test_hdf5_model():
    """Test the HDF5 model with the tall.h5 file"""
    try:
        # Get the URL to the test file
        url = os.path.join(data_url(), "hdf5", "tall.h5")
        log.debug("Testing with file: %s" % url)
        
        # Create a store
        s = store(HDF5Store, url)
        log.debug("Created store: %s" % s)
        
        # Create a container
        c = container(HDF5Store, url, HDF5Group, "/")
        log.debug("Created container: %s" % c)
        
        # List the contents of the container
        log.debug("Container contents:")
        for item in c:
            log.debug("  %s" % item.display_name)
            
        return True
    except Exception as e:
        log.error("Test failed: %s" % str(e))
        return False

if __name__ == "__main__":
    success = test_hdf5_model()
    sys.exit(0 if success else 1) 