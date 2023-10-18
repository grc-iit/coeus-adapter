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
#     spack install coeus-adapter-git
#
# You can edit this file again by typing:
#
#     spack edit coeus-adapter-git
#
# See the Spack documentation for more information on packaging.
# ----------------------------------------------------------------------------

from spack.package import *

class Coeus(CMakePackage):
    homepage = "https://github.com/grc-iit/coeus-adapter"
    url = "https://github.com/grc-iit/coeus-adapter"
    git = "https://github.com/grc-iit/coeus-adapter"

    maintainers("JaimeCernuda")

    version('master', branch='master', submodules=True)

    depends_on("hermes@master")
    depends_on("adios2")
    depends_on("sqlite")

    variant('tracing', default=False, description='Enable tracing through spdlog')

    def cmake_args(self):
        args = ['-DCMAKE_INSTALL_PREFIX={}'.format(self.prefix)]
        if '+tracing' in self.spec:
            args.append('-DENABLE_TRACING')
        return args

    def set_include(self, env, path):
        env.append_flags('CFLAGS', '-I{}'.format(path))
        env.append_flags('CXXFLAGS', '-I{}'.format(path))
        env.prepend_path('INCLUDE', '{}'.format(path))
        env.prepend_path('CPATH', '{}'.format(path))

    def set_lib(self, env, path):
        env.prepend_path('LIBRARY_PATH', path)
        env.prepend_path('LD_LIBRARY_PATH', path)
        env.append_flags('LDFLAGS', '-L{}'.format(path))

    def set_flags(self, env):
        self.set_include(env, '{}/include'.format(self.prefix))
        self.set_include(env, '{}/include'.format(self.prefix))
        self.set_lib(env, '{}/lib'.format(self.prefix))
        self.set_lib(env, '{}/lib64'.format(self.prefix))
        env.prepend_path('CMAKE_PREFIX_PATH', '{}/cmake'.format(self.prefix))

    def setup_dependent_environment(self, spack_env, run_env, dependent_spec):
        self.set_flags(spack_env)

    def setup_run_environment(self, env):
        self.set_flags(env)
