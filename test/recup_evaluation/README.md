# instruction to set the COESU/RECUP evaluations

## software installation
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

```
git clone -b coeus-hash https://github.com/lizdulac/ADIOS2.git
cd ADIOS2
mkdir build
cd build
cmake ../ -D ADIOS2_USE_Kokkos=ON  -D CMAKE_INSTALL_PREFIX=/mnt/common/hxu40/install2 -D StateDiff_ROOT=/mnt/common/hxu40/install2 -D ADIOS2_USE_Derived_Variable=ON -D ADIOS2_USE_SST=OFF -D CMAKE_POSITION_INDEPENDENT_CODE=TRUE -D BUILD_SHARED_LIBS=ON -D BUILD_TESTING=ON
make -j8
make isntall
 ```

4. install coeus-adapter
```
git clone -b derived_merged https://github.com/grc-iit/coeus-adapter.git
spack load hermes
make sure adios2 in the path
module load openmpi
cd coeus-adapter
mkdir build
cd build
cmake ../
make -j8
```

## ADIOS2 hashing operation

## Run the evaluation scripts.
this is the example for jarvis pipleline tests
```
config:
  name: coesu_recup_case_1_eva_1
  env: ??
  pkgs:
    - pkg_type: adios2_gray_scott
      pkg_name: adios2_gray_scott
      path: ${HOME}/coeus-adapter/build/bin/adios2-gray-scott
      out_file=/hdd/out1.bp
      ppn: 20
      L: 512
      steps: 40
      engine: bp5
      nprocs: 1
      ppn: 20
    - pkg_type: adios_hashing
      pkg_name: adios_hashing
      path: ${HOME}/coeus-adapter/build/bin/adios2-hashing
      nprocs: 1
      in_filename: /hdd/out1.bp
      out_filename: /hdd/hashing1.bp 
      ppn: 20
vars:
  adios2_gray_scott.nprocs: [256, 128, 64, 32, 16, 8, 4, 2, 1]
  adios2_gray_scott.L: [1296, 1024, 832, 640, 512, 408, 324, 256, 203]
  adios_hashing.nprocs: [256, 128, 64, 32, 16, 8, 4, 2, 1]
loop:
  - [adios2_gray_scott.nprocs, adios2_gray_scott.L, adios_hashing.nprocs]
repeat: 1
output: "${SHARED_DIR}/output_multi"

```

## shell scripts
please use case1.sh and case2.sh, case3.sh for evaluation in this folder<br>
before use the scripts, please make sure that the output location ```in_filename, out_file``` are set properly <br>
and also be careful for the ``` location="hdd" ```, this means the test is in the HDD, you can also switch to
ssd.

```
#!/bin/bash

# Define variables
L_values=(1296 1024 832 640 512 408 324 256 203)
nprocs_values=(256 128 64 32 16 8 4 2 1)
location="hdd"
report="case1_eva_1"
jarvis repo add ~/coeus/
jarvis repo create case1_eva_1 app
jarvis cd case1_eva_1
jarvis ppl append adios2_gray_scott engine=bp5
jarvis ppl append adios_hashing engine=bp5
jarvis repo create hashing_compare
jarvis ppl append hashing_compare
for i in ${!L_values[@]}; do
  L=${L_values[$i]}
  nprocs=${nprocs_values[$i]}
  jarvis cd case1_eva_1
  # First configuration and run
  mkdir -p ~/${report}/${nprocs}process
  jarvis pkg config adios2_gray_scott out_file=/mnt/${location}/hxu40/ofs-mount/case1/${nprocs}process/out1.bp ppn=20 nprocs=$nprocs steps=40 L=$L checkpoint_output=/mnt/${location}/hxu40/ofs-mount/case1/ckpt.bp
  jarvis pkg config adios_hashing in_filename=/mnt/${location}/hxu40/ofs-mount/case1/${nprocs}process/out1.bp ppn=20 nprocs=$nprocs out_filename=/mnt/${location}/hxu40/ofs-mount/case1/${nprocs}process/copy1.bp
  jarvis ppl run >> ~/${report}/${nprocs}process/result1.txt
  rm -r /mnt/${location}/hxu40/ofs-mount/case1/${nprocs}process/out1.bp

  # Second configuration and run
  jarvis pkg config adios2_gray_scott out_file=/mnt/${location}/hxu40/ofs-mount/case1/${nprocs}process/out2.bp ppn=20 nprocs=$nprocs steps=40 L=$L checkpoint_output=/mnt/${location}/hxu40/ofs-mount/case1/ckpt.bp
  jarvis pkg config adios_hashing in_filename=/mnt/${location}/hxu40/ofs-mount/case1/${nprocs}process/out2.bp ppn=20 nprocs=$nprocs out_filename=/mnt/${location}/hxu40/ofs-mount/case1/${nprocs}process/copy2.bp
  jarvis ppl run >> ~/${report}/${nprocs}process/result2.txt
  rm -r /mnt/${location}/hxu40/ofs-mount/case1/${nprocs}process/out2.bp

  # Hashing compare
  jarvis cd hashing_compare
  jarvis pkg config hashing_compare nprocs=$nprocs ppn=20 in_filename=/mnt/${location}/hxu40/ofs-mount/case1/${nprocs}process/copy1.bp out_filename=/mnt/${location}/hxu40/ofs-mount/case1/${nprocs}process/copy2.bp
  jarvis ppl run >> ~/${report}/${nprocs}process/result3.txt
  rm -r /mnt/${location}/hxu40/ofs-mount/case1/${nprocs}process

done

```