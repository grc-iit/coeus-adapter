import os
from spack.package import *
class Incompact3d(CMakePackage):
    """Xcompact3d is a Fortran-based framework of high-order finite-difference
    flow solvers dedicated to the study of turbulent flows."""

    homepage = "https://github.com/xcompact3d/Incompact3d.git"
    git      = "https://github.com/xcompact3d/Incompact3d.git"

    # Software license
    license('BSD-3-Clause')

    version('coeus', branch='master')

    # Dependencies
    depends_on('mpi')
    depends_on('cmake@3.20:', type='build')
    depends_on('fftw', when='fft_backend=generic')
    depends_on('mkl', when='fft_backend=mkl')
    depends_on('adios2-coeus', when='io_backend=adios2')

    conflicts('%gcc@:8.99', msg='Requires GCC 9 or higher')
    # Read the file content

    # Replace the old path with the dynamic one


    # Write the modified content back

    # def patch(self):
    #     """Dynamically replace hardcoded path in the patch file before applying."""
    #     patch_file = os.path.join(self.package_dir, 'add_path_command.patch')
    #
    #     # Define the old and new path
    #     old_path = '/path/to/decomp_fix.patch'
    #     new_path = self.package_dir + '/patches/decomp_fix.patch'  # Ensure trailing slash
    #
    #     # Read and replace content
    #     with open(patch_file, 'r') as f:
    #         content = f.read()
    #     content = content.replace(old_path, new_path)
    #     with open(patch_file, 'w') as f:
    #         f.write(content)


    patch('add_path_command.patch', when='@coeus')
    patch('change_cmake.patch', when='@coeus')
    variant('fft_backend', default='generic',
        description='FFT backend for 2DECOMP&FFT',
        values=('generic', 'mkl'), multi=False)

   # the io_backend has adios2 and mpiio
    variant('io_backend', default='mpiio',
        description='IO backend',
        values=('mpiio', 'adios2'), multi=False)

    variant('full_testing', default=False,
        description='Enable full testing suite')

def cmake_args(self):
    return [
        self.define_from_variant('FFT_BACKEND', 'fft_backend'),
        self.define_from_variant('IO_BACKEND', 'io_backend'),
    ]

def setup_build_environment(self, env):
    env.set('FC', self.spec['mpi'].mpifc)