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
Implementation of compass_model classes for HDF5 files.
"""
from __future__ import absolute_import, division, print_function, unicode_literals

from itertools import groupby
import sys
import os.path as op
import posixpath as pp

import h5py

import logging
log = logging.getLogger(__name__)
log.addHandler(logging.NullHandler())

# Py2App can't successfully import otherwise
from hdf_compass import compass_model
from hdf_compass.utils import url2path


def sort_key(name):
    """ Sorting key for names in an HDF5 group.

    We provide "natural" sort order; e.g. "7" comes before "12".
    """
    return [(int(''.join(g)) if k else ''.join(g)) for k, g in groupby(name, key=str.isdigit)]


class HDF5Store(compass_model.Store):
    """
    Data store implementation using an HDF5 file.

    Keys are the full names of objects in the file.
    """
    @staticmethod
    def plugin_name():
        return "HDF5"

    @staticmethod
    def plugin_description():
        return "A plugin used to browse HDF5 files."

    file_extensions = {'HDF5 File': ['*.hdf5', '*.h5']}

    def __contains__(self, key):
        return key in self.f

    @property
    def url(self):
        return self._url

    @property
    def display_name(self):
        return op.basename(self.f.filename)

    @property
    def root(self):
        return self['/']

    @property
    def valid(self):
        return bool(self.f)

    @staticmethod
    def can_handle(url):
        if not url.startswith('file://'):
            log.debug("able to handle %s? no, not starting with file://" % url)
            return False
        try:
            path = url2path(url)
            log.debug("Attempting to open file at path: %s" % path)
            if not op.exists(path):
                log.debug("File does not exist at path: %s" % path)
                return False
            if not h5py.is_hdf5(path):
                log.debug("File is not a valid HDF5 file: %s" % path)
                return False
            log.debug("Successfully verified HDF5 file: %s" % path)
            return True
        except Exception as e:
            log.debug("Error handling file %s: %s" % (url, str(e)))
            return False

    def __init__(self, url):
        if not self.can_handle(url):
            raise ValueError(url)
        self._url = url
        path = url2path(url)
        try:
            log.debug("Opening HDF5 file: %s" % path)
            self.f = h5py.File(path, 'r')
            log.debug("Successfully opened HDF5 file: %s" % path)
            log.debug("File contents: %s" % list(self.f.keys()))
        except Exception as e:
            log.error("Failed to open HDF5 file %s: %s" % (path, str(e)))
            raise

    def close(self):
        self.f.close()

    def get_parent(self, key):
        # HDFCompass requires the parent of the root container be None
        if key == "" or key == "/":
            return None
        pkey = pp.dirname(key)
        if pkey == "":
            pkey = "/"
        return self[pkey]


class HDF5Group(compass_model.Container):
    """ Represents an HDF5 group, to be displayed in the browser view. """

    class_kind = "HDF5 Group"

    @staticmethod
    def can_handle(store, key):
        return key in store and isinstance(store.f[key], h5py.Group)

    @property
    def _names(self):

        # Lazily build the list of names; this helps when browsing big files
        if self._xnames is None:

            self._xnames = list(self._group)

            # Natural sort is expensive
            if len(self._xnames) < 1000:
                self._xnames.sort(key=sort_key)

        return self._xnames

    def __init__(self, store, key):
        self._store = store
        self._key = key
        self._group = store.f[key]
        self._xnames = None

    @property
    def key(self):
        return self._key

    @property
    def store(self):
        return self._store

    @property
    def display_name(self):
        name = pp.basename(self.key)
        if name == "":
            name = '/'
        return name

    @property
    def display_title(self):
        return "%s %s" % (self.store.display_name, self.key)

    @property
    def description(self):
        return 'Group "%s" (%d members)' % (self.display_name, len(self))

    def __len__(self):
        try:
            # Check if store is still valid (not closed during shutdown)
            if not self.store.valid:
                log.debug("Store is no longer valid, returning 0 for group length")
                return 0
            return len(self._group)
        except Exception as e:
            log.debug(f"Error getting group length (likely due to shutdown): {e}")
            return 0

    def __iter__(self):
        for name in self._names:
            yield self.store[pp.join(self.key, name)]

    def __getitem__(self, idx):
        try:
            # Check if store is still valid (not closed during shutdown)
            if not self.store.valid:
                log.debug("Store is no longer valid, cannot access items")
                raise IndexError(f"Store is closed, cannot access item {idx}")
                
            if idx < 0 or idx >= len(self._names):
                raise IndexError(f"Index {idx} out of range for container with {len(self._names)} items")
            name = self._names[idx]
            full_key = pp.join(self.key, name)
            return self.store[full_key]
        except Exception as e:
            log.exception(f"Error accessing item {idx} in HDF5Group {self.key}: {e}")
            raise


class HDF5Dataset(compass_model.Array):
    """ Represents an HDF5 dataset. """

    class_kind = "HDF5 Dataset"

    @staticmethod
    def can_handle(store, key):
        return key in store and isinstance(store.f[key], h5py.Dataset)

    def __init__(self, store, key):
        self._store = store
        self._key = key
        self._dset = store.f[key]

    @property
    def key(self):
        return self._key

    @property
    def store(self):
        return self._store

    @property
    def display_name(self):
        return pp.basename(self.key)

    @property
    def description(self):
        return 'Dataset "%s"' % (self.display_name,)

    @property
    def shape(self):
        return self._dset.shape

    @property
    def dtype(self):
        try:
            return self._dset.dtype
        except TypeError:
            # dtype may not be accessible for some types (e.g., big-endian complex)
            # Return None to indicate dtype is not available
            return None

    def __getitem__(self, args):
        return self._dset[args]

    def is_plottable(self):
        try:
            if self.dtype is None:
                return True  # Unknown dtype, let it try to plot
            if self.dtype.kind == 'S':
                log.debug("Not plottable since ASCII String (characters: %d)" % self.dtype.itemsize)
                return False
            if self.dtype.kind == 'U':
                log.debug("Not plottable since Unicode String (characters: %d)" % self.dtype.itemsize)
                return False
            return True
        except (TypeError, AttributeError):
            # If we can't determine dtype.kind, assume it's plottable
            return True


class HDF5Text(compass_model.Text):
    """ Represents a text array (both ASCII and UNICODE). """

    class_kind = "HDF5 Dataset[text]"

    @staticmethod
    def can_handle(store, key):
        if key in store and isinstance(store.f[key], h5py.Dataset):
            try:
                if store.f[key].dtype.kind == 'S':
                    # log.debug("ASCII String (characters: %d)" % DATA[key].dtype.itemsize)
                    return True
                if store.f[key].dtype.kind == 'U':
                    # log.debug("Unicode String (characters: %d)" % DATA[key].dtype.itemsize)
                    return True
            except (TypeError, AttributeError):
                # dtype.kind may not be accessible for some types (e.g., big-endian complex)
                return False

        return False

    def __init__(self, store, key):
        self._store = store
        self._key = key
        self.data = store.f[key]

    @property
    def key(self):
        return self._key

    @property
    def store(self):
        return self._store

    @property
    def display_name(self):
        return pp.basename(self.key)

    @property
    def description(self):
        return 'Text "%s"' % (self.display_name,)

    @property
    def shape(self):
        return self.data.shape

    @property
    def text(self):
        txt = str()

        if len(self.shape) == 0:
            # print(type(self.data))
            txt += str(self.data[()])

        elif len(self.shape) == 1:
            for el in self.data:
                txt += str(el) + ", \n"

        elif len(self.shape) == 2:
            for i in range(self.shape[0]):
                for j in range(self.shape[1]):
                    txt += str(self.data[i, j]) + ", "
                txt += "\n"

        else:
            txt = ">> display of more than 2D string array not implemented <<"

        return txt


class HDF5KV(compass_model.KeyValue):
    """ A KeyValue node used for HDF5 attributes. """

    class_kind = "HDF5 Attributes"

    @staticmethod
    def can_handle(store, key):
        return key in store.f

    def __init__(self, store, key):
        self._store = store
        self._key = key
        self._obj = store.f[key]
        self._names = list(self._obj.attrs.keys())

    @property
    def key(self):
        return self._key

    @property
    def store(self):
        return self._store

    @property
    def display_name(self):
        n = pp.basename(self.key)
        return n if n != '' else '/'

    @property
    def description(self):
        return self.display_name

    @property
    def keys(self):
        return self._names[:]

    def __getitem__(self, name):
        return self._obj.attrs[name]


class HDF5Image(compass_model.Image):
    """
    True-color images.
    """

    class_kind = "HDF5 Truecolor Image"

    @staticmethod
    def can_handle(store, key):
        if key not in store:
            return False
        obj = store.f[key]
        if obj.attrs.get('CLASS') != 'IMAGE':
            return False
        if obj.attrs.get('IMAGE_SUBCLASS') != 'IMAGE_TRUECOLOR':
            return False
        if obj.attrs.get('INTERLACE_MODE') != 'INTERLACE_PIXEL':
            return False
        return True

    def __init__(self, store, key):
        self._store = store
        self._key = key
        self._obj = store.f[key]

    @property
    def key(self):
        return self._key

    @property
    def store(self):
        return self._store

    @property
    def display_name(self):
        n = pp.basename(self.key)
        return n if n != '' else '/'

    @property
    def description(self):
        return self.display_name

    @property
    def width(self):
        return self._obj.shape[1]

    @property
    def height(self):
        return self._obj.shape[0]

    @property
    def palette(self):
        return None

    @property
    def data(self):
        return self._obj[:]


# Register handlers    
HDF5Store.push(HDF5KV)
HDF5Store.push(HDF5Dataset)
HDF5Store.push(HDF5Text)
HDF5Store.push(HDF5Group)
HDF5Store.push(HDF5Image)

compass_model.push(HDF5Store)
