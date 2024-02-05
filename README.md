# COEUS Hermes Adios Engine

This work is an adapter which connects ADIOS to Hermes through the use
of the ADIOS plugins interface.

## Install

To compile


```
git clone https://github.com/lukemartinlogan/coeus-adapter.git
spack load hermes@master
spack load adios2
cd coeus-adapter
mkdir build
cd build
cmake ../
make -j8
```
Note:
To enable metadata and function trace feature, please comment out /src/CMakeLists.txt
```
#add_definitions(-DMeta_enabled)
#add_definitions(-Ddebug_mode)
```
## Test

To test the functionality of the adapter, run:
```
ctest
```
