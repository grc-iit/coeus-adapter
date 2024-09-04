# COEUS Hermes Adios Engine

This work is an adapter which connects ADIOS to Hermes through the use
of the ADIOS plugins interface.

## Dependencies

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
To enable the metadata and function trace features, please add the appropriate flags during the CMake configuration.
```
cmake .. -Dmeta_enabled=ON -Ddebug_mode=ON
```
## Test

To test the functionality of the adapter, run:
```
ctest
```
## Derived Variable
To use the Adios2 derived variable feature, please switch to the derived_merged branch and install the latest version of Adios2 with the derived feature enabled.
1. install kokkos with enable threads, ```spack install kokkos```,
2.  install statediff https://github.com/DataStates/state-diff/tree/coeus,   add these config:
   ```
-D BUILD_SHARED_LIBS=ON
-D CMAKE_INSTALL_PREFIX="${INSTALL_DIR}
```
4.  modify adios2 packages.py, and change the URL and branch. https://github.com/lizdulac/ADIOS2/tree/coeus_hash

## Hermes Info log
The Hermes info log is disabled by default. To enable the Hermes log, please set log_verbosity = 1 in hermes_run.



## manually install adios2 for derived variables

1. install kokkos:
```
  git clone -b develop  https://github.com/kokkos/kokkos.git
  cd kokkos
  mkdir build
  cmake ../ -D CMAKE_INSTALL_PREFIX=/mnt/common/hxu40/install2  -D Kokkos_ENABLE_SERIAL=ON -D CMAKE_CXX_STANDARD=17 -D CMAKE_POSITION_INDEPENDENT_CODE=TRUE -D BUILD_SHARED_LIBS=ON -D 
  Kokkos_ENABLE_THREAD=ON
  make -j8
  make install

```


2. install state-diff
  ```
 mkdir build
  cmake ../ -D CMAKE_BUILD_TYPE=RelWithDebInfo  -D CMAKE_INSTALL_PREFIX=/mnt/common/hxu40/install2 -D Kokkos_ROOT=/mnt/common/hxu40/install2 -D CMAKE_POSITION_INDEPENDENT_CODE=TRUE -D BUILD_SHARED_LIBS=ON
  make -j8
  make install
  ```



3. install adios2@coeus_hash
   first, comment out the source/adios2/Cmakefile.txt from line from line 144 to 162
   
   ```
   find_package(BISON "3.8.2")
  find_package(FLEX)

 //  if(NOT BISON_FOUND OR NOT FLEX_FOUND)
    include(ADIOSBisonFlexSub)
    SETUP_ADIOS_BISON_FLEX_SUB()
 //  else()
 //   BISON_TARGET(MyParser
 //     toolkit/derived/parser/parser.y
 //     ${CMAKE_CURRENT_BINARY_DIR}/parser.cpp
 //     COMPILE_FLAGS "-o parser.cpp --header=parser.h"
 //     DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/parser.h)
//   FLEX_TARGET(MyScanner
//     toolkit/derived/parser/lexer.l
 //     COMPILE_FLAGS "-o lexer.cpp --header-file=lexer.h" 
 //     ${CMAKE_CURRENT_BINARY_DIR}/lexer.cpp
 //     DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/lexer.h)
//   ADD_FLEX_BISON_DEPENDENCY(MyScanner MyParser)
 // endif()
 ```

```
git clone -b coeus-hash https://github.com/lizdulac/ADIOS2.git
cd ADIOS2
mkdir build
cd build
cmake ../ -D ADIOS2_USE_Kokkos=ON  -D CMAKE_INSTALL_PREFIX=/mnt/common/hxu40/install2 -D StateDiff_ROOT=/mnt/common/hxu40/install2 -D ADIOS2_USE_Derived_Variable=ON -D ADIOS2_USE_SST=OFF -D CMAKE_POSITION_INDEPENDENT_CODE=TRUE -D BUILD_SHARED_LIBS=ON -D BUILD_TESTING=ON
make -j8
make isntall
 ```
