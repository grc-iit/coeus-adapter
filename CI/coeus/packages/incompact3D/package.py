class Incompact3d(CMakePackage):
    """Xcompact3d is a Fortran-based framework of high-order finite-difference
    flow solvers dedicated to the study of turbulent flows."""

    homepage = "https://github.com/xcompact3d/Incompact3d.git"
    git      = "https://github.com/xcompact3d/Incompact3d.git"

    # Software license
    license('BSD-3-Clause')

    version('coeus', branch='master')
    def patch(self):
        # Copy the patch file into the CMake source tree
        patch_src = os.path.join(self.package_dir, 'patches', 'decomp_fix.patch')
        patch_dst = os.path.join(self.stage.source_path, 'cmake', 'patches', 'decomp_fix.patch')

        mkdirp(os.path.dirname(patch_dst))
        shutil.copy(patch_src, patch_dst)
    # Dependencies
    depends_on('mpi')
    depends_on('cmake@3.20:', type='build')
    depends_on('fftw', when='fft_backend=generic')
    depends_on('mkl', when='fft_backend=mkl')
    depends_on('adios2-coeus', when='io_backend=adios2')

    conflicts('%gcc@:8.99', msg='Requires GCC 9 or higher')
    # change the 2decomp-fft repo to cutomized repos
    patch('add_path_command.patch', when='@coeus')
    patch('patches/decomp_fix.patch', when='@coeus')
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