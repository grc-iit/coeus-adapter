
## How to install it

### MPI I/O installation
```
git clone https://github.com/xcompact3d/Incompact3d
export FC=my_mpif90
cmake -S . -B build
```


### ADIOS2 I/O installation

```
git clone -b coeus https://github.com/hxu65/2decomp-fft.git
cmake -S . -B ./build -DIO_BACKEND=adios2 -Dadios2_DIR=/mnt/common/hxu40/install2/lib/cmake/adios2
spack install intel-oneapi-mkl
export MKL_DIR=/mnt/common/hxu40/spack/opt/spack/linux-ubuntu22.04-skylake_avx512/gcc-11.4.0/intel-oneapi-mkl-2024.2.2-z5q74r7t24qiimwlklk6jofy5twcmsjq/mkl/latest/lib/cmake/mkl
cmake -S . -B ./build -DIO_BACKEND=adios2 -DCMAKE_PREFIX_PATH=/mnt/common/hxu40/software/2decomp-fft/build -Dadios2_DIR=/mnt/common/hxu40/install2/lib/cmake/adios2
cd build
make install
```

### adios2 BP5 modification
In /2decomp-fft/src/io.f90, change all BP4 file to BP5 file. 
### run
```
cd Incompact3D/examples/Channel
mpirun -n 16 ../../build/bin/incompcat3d
```