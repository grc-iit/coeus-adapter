
## How to install it

### MPI I/O installation
```
git clone https://github.com/xcompact3d/Incompact3d
export FC=my_mpif90
cmake -S . -B build
```


### ADIOS2 I/O installation
2decomp-fft is responsible for domain decomposition and parallel I/O, Incompact3D relies on it for writing field data. </br>
Here is the installation of 2decomp-fft with adios2 support
```
git clone -b coeus https://github.com/hxu65/2decomp-fft.git
spack load intel-oneapi-mkl
spack load openmpi
export MKL_DIR=/mnt/common/hxu40/spack/opt/spack/linux-ubuntu22.04-skylake_avx512/gcc-11.4.0/intel-oneapi-mkl-2024.2.2-z5q74r7t24qiimwlklk6jofy5twcmsjq/mkl/latest/lib/cmake/mkl
cmake -S . -B ./build -DIO_BACKEND=adios2 -DCMAKE_PREFIX_PATH=/mnt/common/hxu40/software/2decomp-fft/build -Dadios2_DIR=/mnt/common/hxu40/install2/lib/cmake/adios2
cd build
make -j8
make install
```
Build Incompact3D with adios2 support
```
git clone https://github.com/xcompact3d/Incompact3d
cd Incompact3d
spack load intel-oneapi-mkl
spack load openmpi
export MKL_DIR=${MKLROOT}/lib/cmake/mkl
cmake -S . -B ./build -DIO_BACKEND=adios2 -Dadios2_DIR=/path/to/adios2/install/lib/cmake/adios2 -Ddecomp2d_DIR=/path/to/decomp2d/build

```

 
### run
```
cd Incompact3D/examples/Channel
mpirun -n 16 ../../build/bin/incompcat3d
```