for the installation, please refer to jarvis Incompact3D

This is the instrmentation on how to use catalyst for adios2 as I/O engine

Paraview installation

```
spack install paraview@5.13.3 +adios2^python + qt +fides +mpi +libcatalyst +python ^py-mpi4py ^python-venv
```
Please note that the plugin uses the ADIOS inline engine to pass data pointers to ParaView's Fides reader and uses ParaView Catalyst to process a user python script that contains a ParaView pipeline. Fides is a library that provides a schema for reading ADIOS data into visualization services such as ParaView. By integrating it with ParaView Catalyst, it is now possible to perform in situ visualization with ADIOS2-enabled codes without writing adaptors.


## environment setup
```
$ export ADIOS2_PLUGIN_PATH=/path/to/adios2-build/lib
$ export CATALYST_IMPLEMENTATION_NAME=paraview
$ export CATALYST_IMPLEMENTATION_PATHS=/path/to/paraview-build/lib/catalyst
```

## run the experiment
```
mpirun -n 4 xcompact3D
```