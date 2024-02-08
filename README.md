# COEUS Hermes Adios Engine

This work is an adapter which connects ADIOS to Hermes through the use
of the ADIOS plugins interface.

## Dependencies

* A C++17 compiler, GCC 9.4 is the minimum tested version.
* [Hermes](https://github.com/HDFGroup/hermes): a multi-tiered I/O buffering platform.
* [ADIOS2](https://github.com/ornladios/ADIOS2): an I/O library

### ADIOS2

We have found some cases where spack failed to install ADIOS2. Below are
instructions on installing ADIOS2 from source. SCSPKG is a system for
organizing manually-built software and producing modulefiles for them.
Instructions to install are [here](https://grc.iit.edu/docs/hermes/building-hermes#optional-create-a-hermes-scspkg-repo).

```bash
scspkg create adios2
cd $(scspkg pkg src adios2)
git clone https://github.com/ornladios/ADIOS2.git
cd ADIOS2
mkdir build
cd build
cmake ../ \
-DCMAKE_INSTALL_PREFIX=$(scspkg pkg root adios2) \
-DCMAKE_CXX_COMPILER=g++ \
-DCMAKE_C_COMPILER=gcc \
-DADIOS2_USE_SST=OFF \
-DBUILD_DOCS=ON \
-DBUILD_TESTING=ON \
-DADIOS2_BUILD_EXAMPLES=ON \
-DADIOS2_USE_MPI=ON \
-DADIOS2_USE_Derived_Variable=ON
make -j32 install
```

## Install

Load dependencies:
```bash
spack load hermes
module load adios2
```

To compile:
```
git clone https://github.com/grc-iit/coeus-adapter.git
cd coeus-adapter
mkdir build
cd build
cmake ../
make -j8
```

Note:
To enable metadata and function trace feature, please add flag during cmake
```
cmake .. -Dmeta_enabled=ON -Ddebug_mode=ON
```
## Test

To test the functionality of the adapter, run:
```
ctest
```
