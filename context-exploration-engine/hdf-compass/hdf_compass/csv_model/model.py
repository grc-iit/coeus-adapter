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
HDF Compass plugin for viewing CSV files.

Provides support for reading and viewing CSV files as array data.
"""
from __future__ import absolute_import, division, print_function, unicode_literals

import os.path as op
import pandas as pd
import numpy as np
import logging

log = logging.getLogger(__name__)

from hdf_compass import compass_model
from hdf_compass.utils import url2path


class StructuredSlice:
    """
    A wrapper for numpy array slices that supports both integer indexing and structured field access.
    This is needed for LRUTileCache compatibility with structured arrays.
    """
    def __init__(self, data, columns):
        self.data = data
        self.columns = columns
        self.shape = data.shape

    def __getitem__(self, index):
        # Handle tuple indexing (e.g., (0,) from LRUTileCache)
        if isinstance(index, tuple) and len(index) == 1:
            index = index[0]

        # Handle string field name access (for plotting)
        if isinstance(index, str):
            field_name = index
            for i, col in enumerate(self.columns):
                clean_name = str(col).replace("'", "").replace("[", "_").replace("]", "_").replace(" ", "_")
                if clean_name == field_name:
                    return self.data[:, i]
            raise KeyError(f"Field {field_name} not found")

        if isinstance(index, int):
            # Return a structured record-like object for single row access
            row_data = self.data[index, :]
            record = {}
            for i, col in enumerate(self.columns):
                clean_name = str(col).replace("'", "").replace("[", "_").replace("]", "_").replace(" ", "_")
                record[clean_name] = row_data[i]
            return record
        else:
            # For other access patterns, return raw data
            return self.data[index]


class CSVStore(compass_model.Store):
    """A data store represented by a CSV file."""

    @staticmethod
    def plugin_name():
        return "CSV"

    @staticmethod
    def plugin_description():
        return "A plugin used to browse CSV files as array data."

    file_extensions = {'CSV File': ['*.csv'], 'Tab-separated': ['*.tsv'], 'Text Files': ['*.txt']}

    def __contains__(self, key):
        if key == '/':
            return True
        return False

    @property
    def url(self):
        return self._url

    @property
    def display_name(self):
        return op.basename(url2path(self._url))

    @property
    def root(self):
        return self['/']

    @property
    def valid(self):
        return self._valid

    @staticmethod
    def can_handle(url):
        if not url.startswith('file://'):
            log.debug("able to handle %s? no, not starting with file://" % url)
            return False

        path = url2path(url)
        if not op.exists(path):
            log.debug("able to handle %s? no, file does not exist" % url)
            return False

        # Check file extension
        _, ext = op.splitext(path.lower())
        if ext not in ['.csv', '.tsv', '.txt']:
            log.debug("able to handle %s? no, unsupported extension %s" % (url, ext))
            return False

        # Try to read a few lines to validate it's a CSV-like file
        try:
            df = pd.read_csv(path, nrows=5)
            if len(df.columns) == 0:
                log.debug("able to handle %s? no, no columns found" % url)
                return False
            log.debug("able to handle %s? yes" % url)
            return True
        except Exception as e:
            log.debug("able to handle %s? no, pandas read error: %s" % (url, str(e)))
            return False

    def __init__(self, url):
        if not self.can_handle(url):
            raise ValueError(url)
        self._url = url
        self._valid = True

    def close(self):
        self._valid = False

    def get_parent(self, key):
        return None

    def getFilePath(self):
        return url2path(self._url)


class CSVArray(compass_model.Array):
    """Represents CSV data as an array."""

    class_kind = "CSV Data"

    @staticmethod
    def can_handle(store, key):
        return key == '/'

    def __init__(self, store, key):
        self._store = store
        self._key = key
        self._data = None
        self._shape = None
        self._dtype = None
        self._columns = None

        # Load metadata without loading full data
        self._load_metadata()

    def _load_metadata(self):
        """Load just the metadata (shape, columns, dtype) without loading all data."""
        try:
            file_path = self._store.getFilePath()

            # Determine separator
            _, ext = op.splitext(file_path.lower())
            sep = '\t' if ext == '.tsv' else ','

            # Read just a sample to get column info and infer dtypes
            sample_df = pd.read_csv(file_path, sep=sep, nrows=100)

            # Get full row count more efficiently
            with open(file_path, 'r') as f:
                row_count = sum(1 for line in f) - 1  # -1 for header

            self._shape = (row_count, len(sample_df.columns))
            self._columns = sample_df.columns.tolist()

            # Determine overall dtype - use object for mixed types
            if sample_df.dtypes.nunique() == 1:
                # All columns have same type
                self._dtype = sample_df.dtypes.iloc[0]
            else:
                # Mixed types - use object
                self._dtype = np.dtype('object')

        except Exception as e:
            log.error("Error loading CSV metadata: %s" % str(e))
            self._shape = (0, 0)
            self._columns = []
            self._dtype = np.dtype('object')

    def _load_data(self):
        """Load the actual data when needed."""
        if self._data is None:
            try:
                file_path = self._store.getFilePath()

                # Determine separator
                _, ext = op.splitext(file_path.lower())
                sep = '\t' if ext == '.tsv' else ','

                # Load the data
                df = pd.read_csv(file_path, sep=sep)
                self._data = df.values

            except Exception as e:
                log.error("Error loading CSV data: %s" % str(e))
                # Create empty array as fallback
                self._data = np.empty(self._shape, dtype=self._dtype)

    @property
    def key(self):
        return self._key

    @property
    def store(self):
        return self._store

    @property
    def display_name(self):
        return self._store.display_name

    @property
    def description(self):
        return 'CSV file "%s" with %d rows and %d columns' % (
            self.display_name, self._shape[0], self._shape[1])

    @property
    def shape(self):
        # For structured arrays, ArrayTable expects shape to be (num_records,)
        if self._columns:
            return (self._shape[0],)
        return self._shape

    @property
    def dtype(self):
        # Create a structured dtype so ArrayTable treats this as columnar data
        if self._columns:
            dtype_list = []
            for col in self._columns:
                # Clean column names for numpy dtype compatibility
                clean_name = str(col).replace("'", "").replace("[", "_").replace("]", "_").replace(" ", "_")
                dtype_list.append((clean_name, 'O'))
            return np.dtype(dtype_list)
        return self._dtype

    def __getitem__(self, args):
        self._load_data()

        # Handle structured array access for ArrayTable compatibility
        if isinstance(args, str):
            # Accessing by field name (column name)
            field_name = args
            for i, col in enumerate(self._columns):
                clean_name = str(col).replace("'", "").replace("[", "_").replace("]", "_").replace(" ", "_")
                if clean_name == field_name:
                    return self._data[:, i]
            raise KeyError(f"Field {field_name} not found")

        # Handle single integer index (row access) for structured arrays
        if isinstance(args, int):
            # Return a structured record-like object
            row_data = self._data[args, :]
            # Create a record-like object with named fields
            record = {}
            for i, col in enumerate(self._columns):
                clean_name = str(col).replace("'", "").replace("[", "_").replace("]", "_").replace(" ", "_")
                record[clean_name] = row_data[i]
            return record

        # Handle slice access
        if isinstance(args, slice):
            # Return multiple rows as a custom object that supports both array and structured access
            slice_data = self._data[args, :]
            return StructuredSlice(slice_data, self._columns)

        # Handle tuple slice access (for LRUTileCache)
        if isinstance(args, tuple) and len(args) == 1 and isinstance(args[0], slice):
            slice_data = self._data[args[0], :]
            return StructuredSlice(slice_data, self._columns)

        # Default behavior for other access patterns
        return self._data[args]


class CSVKeyValue(compass_model.KeyValue):
    """Key-value view of CSV metadata."""

    class_kind = "CSV Metadata"

    @staticmethod
    def can_handle(store, key):
        return key == '/'

    def __init__(self, store, key):
        self._store = store
        self._key = key

        # Get file metadata
        file_path = store.getFilePath()
        import os
        stat = os.stat(file_path)

        # Read sample to get column info
        try:
            _, ext = op.splitext(file_path.lower())
            sep = '\t' if ext == '.tsv' else ','
            sample_df = pd.read_csv(file_path, sep=sep, nrows=5)
            columns = sample_df.columns.tolist()
            dtypes = {str(col): str(dtype) for col, dtype in sample_df.dtypes.items()}
        except Exception:
            columns = []
            dtypes = {}

        self.data = {
            'File size (bytes)': stat.st_size,
            'Number of columns': len(columns),
            'Column names': ', '.join(columns) if columns else 'Unknown',
            'Column types': str(dtypes) if dtypes else 'Unknown',
            'Separator': 'Tab' if ext == '.tsv' else 'Comma',
        }

    @property
    def key(self):
        return self._key

    @property
    def store(self):
        return self._store

    @property
    def display_name(self):
        return "CSV Info"

    @property
    def description(self):
        return "Metadata for CSV file"

    @property
    def keys(self):
        return list(self.data.keys())

    def __getitem__(self, key):
        return self.data[key]


# Register the components
CSVStore.push(CSVKeyValue)  # metadata
CSVStore.push(CSVArray)     # array data

compass_model.push(CSVStore)