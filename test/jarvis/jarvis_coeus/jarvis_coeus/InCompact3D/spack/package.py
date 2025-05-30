class Incompact3d(CMakePackage):
    """Xcompact3d is a Fortran-based framework of high-order finite-difference
    flow solvers dedicated to the study of turbulent flows."""

    homepage = "https://github.com/xcompact3d/Incompact3d"
    git      = "https://github.com/xcompact3d/Incompact3d.git"
    url      = "https://github.com/xcompact3d/Incompact3d/archive/refs/tags/v5.0.tar.gz"

    # Software license
    license('BSD-3-Clause')

    version('5.0', commit='06af91a0713a362af42ae7b04d491260d31b70ea')
    version('4.1', commit='2da7699558806e70574a6980a75193a80f5734c9')
    version('4.0', commit='2546f404cf3bbf78bc745d6133173c37e46c82df')
    version('3.0', commit='90157650e8a8162fd0ac3d5f7dba918a994d80c9')
    version('2.0.0', commit='ae5d58d1bc0160be55a5f26e77853711bce97bc9')

    # Dependencies
    depends_on('mpi')
    depends_on('cmake@3.20:', type='build')

    # Compiler requirements
    conflicts('%gcc@:8.99', msg='Requires GCC 9 or higher')

    # Variants for optional features
    variant('fft_backend', default='generic',
            description='FFT backend for 2DECOMP&FFT',
            values=('generic', 'mkl'), multi=False)
    variant('io_backend', default='mpiio',
            description='IO backend',
            values=('mpiio', 'adios2'), multi=False)
    variant('full_testing', default=False,
            description='Enable full testing suite')

    # MKL dependency when using mkl fft backend
    depends_on('mkl', when='fft_backend=mkl')
    depends_on('adios2', when='io_backend=adios2')

    def cmake_args(self):
        args = []

        # FFT backend configuration
        if self.spec.variants['fft_backend'].value == 'mkl':
            args.append('-DFFT_BACKEND=mkl')

        # IO backend configuration
        if self.spec.variants['io_backend'].value == 'adios2':
            args.append('-DIO_BACKEND=adios2')

        # Testing configuration
        if self.spec.variants['full_testing'].value:
            args.append('-DBUILD_TESTING_FULL=ON')

        return args

    def setup_build_environment(self, env):
        # Set the Fortran compiler to the MPI wrapper
        env.set('FC', self.spec['mpi'].mpifc)