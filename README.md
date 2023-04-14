# COEUS Hermes Adios Engine

This work is an adapter which connects ADIOS to Hermes through the use
of the ADIOS plugins interface.

## Install

```
git clone https://github.com/lukemartinlogan/coeus-adapter.git
cd coeus-adapter
mkdir build
cd build
cmake ../
make -j8
```

## Test

To test the functionality of the adapter, run:
```
ctest
```