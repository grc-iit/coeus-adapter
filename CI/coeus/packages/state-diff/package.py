# Copyright 2013-2023 Lawrence Livermore National Security, LLC and other
# Spack Project Developers. See the top-level COPYRIGHT file for details.
#
# SPDX-License-Identifier: (Apache-2.0 OR MIT)

# ----------------------------------------------------------------------------
# If you submit this package back to Spack as a pull request,
# please first remove this boilerplate and all FIXME comments.
#
# This is a template package file for Spack.  We've put "FIXME"
# next to all the things you'll want to change. Once you've handled
# them, you can save this file and test your package like this:
#
#     spack install StateDiff
#
# You can edit this file again by typing:
#
#     spack edit StateDiff
#
# See the Spack documentation for more information on packaging.
# ----------------------------------------------------------------------------

from spack.package import *

class StateDiff(CMakePackage):
    """StateDiff: A tool for state difference analysis."""

    homepage = "https://github.com/DataStates/state-diff"
    git      = "https://github.com/DataStates/state-diff.git"

    version('master', branch='master')
    version('devel', branch='devel')
    version('paper', tag='v0.1.0-middleware-2024')

    variant('tests', default=False, description='Enable tests')
    variant('io_uring', default=False, description='Enable IO Uring support')
    variant('kokkos', default=True, description='Enable Kokkos specific flags')

    depends_on('kokkos', when='+kokkos')
    depends_on('liburing', when='+io_uring')
    depends_on('mpi')

    def cmake_args(self):
        args = []
        args.append(self.define_from_variant('ENABLE_TEST   S', 'tests'))
        args.append(self.define_from_variant('ENABLE_IO_URING', 'io_uring'))
        args.append(self.define_from_variant('ENABLE_KOKKOS', 'kokkos'))
        return args

    def setup_run_environment(self, env):
        if '+kokkos' in self.spec:
            env.prepend_path('MODULEPATH', self.spec['kokkos'].prefix)
        if '+io_uring' in self.spec:
            env.prepend_path('MODULEPATH', self.spec['liburing'].prefix)
