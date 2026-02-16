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
Implementation of utils and helper functions
"""
from __future__ import absolute_import, division, print_function, unicode_literals

import sys
import os

import logging
log = logging.getLogger(__name__)

is_darwin = sys.platform == 'darwin'
is_win = sys.platform == 'win32'
is_linux = sys.platform == 'linux2'


def url2path(url):
    """ Helper function that returns the file path from an url, dealing with Windows peculiarities """
    if is_win:
        # Remove 'file:///' prefix and handle Windows paths
        path = url.replace('file://', '')
        # Convert forward slashes to backslashes for Windows
        path = path.replace('/', '\\')
        return path
    else:
        return url.replace('file://', '')


def path2url(path):
    """ Helper function that returns the url from a file path, dealing with Windows peculiarities """
    if is_win:
        # Convert backslashes to forward slashes for URLs
        path = path.replace('\\', '/')
        return 'file:///' + path
    else:
        return 'file://' + path


def data_url():
    """ Helper function used to return the url to the project data folder """
    prj_root_folder = os.path.abspath(os.path.join(os.path.dirname(__file__), os.pardir, os.pardir))
    data_folder = os.path.join(prj_root_folder, "data")

    if not os.path.exists(data_folder):
        raise RuntimeError("data path %s does not exist" % data_folder)

    return path2url(data_folder)
