This is the instrmentation on how to use catalyst for adios2 as I/O engine

ADIOS2 installation

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
mpirun -n 4 build/adios2_simulations_gray-scott simulation/settings-inline.json
```

# Paraview GUI setup

This video shows how to use the Paraview GUI with the catalyst setup:

**[Watch the demonstration video on YouTube](https://youtu.be/FD0nAeOLC8s)**