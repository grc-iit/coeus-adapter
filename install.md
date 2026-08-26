# COEUS-Adapter Installation Guide

## Dependencies

* [clio-core (IOWarp core)](https://github.com/iowarp/clio-core): provides the
  `iowarp-core` package (Chimaera runtime + Context-Transfer-Engine) - the
  backbone I/O engine COEUS builds against.
* [ADIOS2](https://github.com/ornladios/ADIOS2): I/O library, built with
  derived-variable support (the `adios2-coeus` Spack package).
* MPI (OpenMPI recommended).

## 1. Install Spack

```bash
cd ${HOME}
git clone https://github.com/spack/spack.git
cd spack
echo ". ${PWD}/share/spack/setup-env.sh" >> ~/.bashrc
source ~/.bashrc
```

## 2. Install ADIOS2 (with derived variables)

The `adios2-coeus` package lives in this repository's Spack repo:

```bash
git clone https://github.com/grc-iit/coeus-adapter.git ${HOME}/coeus-adapter
spack repo add ${HOME}/coeus-adapter/CI/coeus
spack install adios2-coeus@master
```

## 3. Install IOWarp (clio-core)

The `iowarp` package lives in clio-core's own Spack repo:

```bash
cd ${HOME}
git clone https://github.com/iowarp/clio-core.git
spack repo add clio-core/installers/spack
spack install iowarp@main
```

## 4. Build COEUS-Adapter

1. Load the environment:

```bash
spack load iowarp@main
spack load adios2-coeus
spack load openmpi
```

2. Configure and build:

```bash
cd ${HOME}/coeus-adapter
mkdir build && cd build
cmake .. -Dmeta_enabled=ON -Ddebug_mode=OFF
make -j8
```

The plugin is built at `build/bin/libhermes_engine.so` (the Hermes-era name is
retained for compatibility; the I/O backbone is clio-core's CTE).

Notes:
- `-Dmeta_enabled=ON` enables metadata collection; `-Ddebug_mode=ON` enables
  verbose engine logging.
- If CMake fails with `CMAKE_C_COMPILER not set, after EnableLanguage`, pass
  the compilers explicitly:
  `cmake .. -DCMAKE_C_COMPILER=$(which gcc) -DCMAKE_CXX_COMPILER=$(which g++) ...`
- See [BUILD_GUIDE.md](BUILD_GUIDE.md) for all options and troubleshooting.

## Jarvis installation (unified platform for deploying applications)

1. Install Jarvis:

```bash
spack external find python
spack install py-jarvis-cd
spack load py-jarvis-cd
```

2. Initialize Jarvis (follow this
   [link](https://grc.iit.edu/docs/jarvis/jarvis-cd/index/#initialize-jarvis-configuration))
   for the following steps:
   - Initialize the Jarvis configuration
   - Set or change the active hostfile
   - Set up passwordless SSH
   - Build a resource graph

## Incompact3d installation

The installation, which includes the 2DECOMP&FFT library, enables both slab and
pencil decompositions along with FFT support.

In this setup, we apply a patch to the 2DECOMP&FFT library to add the derived
variable for the Q-criterion.

The Incompact3d application will use ADIOS2 for I/O, with the BP5 engine enabled
through an additional patch. Note: the default Incompact3d uses MPI-IO.

```bash
spack load adios2-coeus@master
spack install incompact3D io_backend=adios2 ^openmpi ^adios2-coeus@master
```

## Py4Incompact3D installation (post-processing tool for raw output)

The raw ADIOS2 BP5 simulation output is used by this program to calculate the
Q-criterion.

```bash
git clone https://github.com/xcompact3d/Py4Incompact3D.git
cd Py4Incompact3D
pip install .
```
