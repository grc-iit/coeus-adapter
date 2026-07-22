spack install adios2 + mpi + libcatalst + python
spack install paraview + fides + libcatalyst + python ^adios
```

export ADIOS2_PLUGIN_PATH=/home/ubuntu/spack/opt/spack/linux-ubuntu22.04-zen3/gcc-11.4.0/adios2-2.10.0-arblokrsuswjiio4v74ho4voindtu6ka/lib

export CATALYST_IMPLEMENTATION_NAME=paraview
export CATALYST_IMPLEMENTATION_PATHS=/home/ubuntu/spack/opt/spack/linux-ubuntu22.04-zen3/gcc-11.4.0/paraview-5.13.1-h5itrsfaotudijqairiqoj4tabqlju2n/lib/catalyst

```