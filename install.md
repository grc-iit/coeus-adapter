## install with spack

### 1.Install ADIOS2
please follow these steps to install the adios2 with derived variables
step 1: Install Spack
```
cd ${HOME}
git clone https://github.com/spack/spack.git
cd spack
git checkout tags/v0.22.2
echo ". ${PWD}/share/spack/setup-env.sh" >> ~/.bashrc
source ~/.bashrc
```
step 2: Add Coeus repo packages for spack
```
spack repo add /coeus_adapter/CI/coeus
```
step 3: install the adios2
```
spack install adios2-coeus@2.10.0
```

### 2. Install the Hermes
Please follow this [link](https://grc.iit.edu/docs/hermes/building-hermes/) for the hermes installation.

### 3. Install Coeus-adapter
1. load environment variables
```
spack load hermes@master
spack load adios2-coeus
spack load openmpi
```
2. install the coeus-adapter
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
## Hermes Info log
The Hermes info log is disabled by default. To enable the Hermes log, please set log_verbosity = 1 in hermes_run.

## Disbale adios2 metadata
add this to the adios2.xml
```
<parameter key="StatsLevel" value="0"/>
```

# coeus-adapter with Hash()

## Manually install adios2 for derived variables

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

3. install adios2@coeus_hash:
```
git clone -b coeus-hash https://github.com/lizdulac/ADIOS2.git
cd ADIOS2
mkdir build
cd build
cmake ../ -D ADIOS2_USE_Kokkos=ON  -D CMAKE_INSTALL_PREFIX=/mnt/common/hxu40/install2 -D StateDiff_ROOT=/mnt/common/hxu40/install2 -D ADIOS2_USE_Derived_Variable=ON -D ADIOS2_USE_SST=OFF -D CMAKE_POSITION_INDEPENDENT_CODE=TRUE -D BUILD_SHARED_LIBS=ON -D BUILD_TESTING=ON
make -j8
make isntall
 ```
Note: if there is error message relate to BISON, please
 comment out the source/adios2/Cmakefile.txt from line from line 144 to 162
```
   find_package(BISON "3.8.2")
  find_package(FLEX)

if(NOT BISON_FOUND OR NOT FLEX_FOUND)
    include(ADIOSBisonFlexSub)
    SETUP_ADIOS_BISON_FLEX_SUB()
 else()
   BISON_TARGET(MyParser
     toolkit/derived/parser/parser.y
     ${CMAKE_CURRENT_BINARY_DIR}/parser.cpp
     COMPILE_FLAGS "-o parser.cpp --header=parser.h"
     DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/parser.h)
 FLEX_TARGET(MyScanner
    toolkit/derived/parser/lexer.l
    COMPILE_FLAGS "-o lexer.cpp --header-file=lexer.h" 
     ${CMAKE_CURRENT_BINARY_DIR}/lexer.cpp
      DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/lexer.h)
   ADD_FLEX_BISON_DEPENDENCY(MyScanner MyParser)
 endif()
``` 
