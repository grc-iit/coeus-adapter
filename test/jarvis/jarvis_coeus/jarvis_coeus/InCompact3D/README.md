
## The Incompact3D tutorial

### MPI I/O installation
```
git clone https://github.com/xcompact3d/Incompact3d
export FC=my_mpif90
cmake -S . -B build
```





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
### Build Incompact3D manually
```
git clone https://github.com/xcompact3d/Incompact3d
cd Incompact3d
spack load intel-oneapi-mkl
spack load openmpi
export MKL_DIR=${MKLROOT}/lib/cmake/mkl
cmake -S . -B ./build -DIO_BACKEND=adios2 -Dadios2_DIR=/path/to/adios2/install/lib/cmake/adios2 -Ddecomp2d_DIR=/path/to/decomp2d/build
cd build
make -j8
make install
```

## install the Incompact3D with spack
After the installation, please save the incompact3D location
```
git clone -b derived-merged https://github.com/grc-iit/coeus-adapter.git
spack repo add /coeus_adapter/CI/coeus
spack install incompact3D io_backend=adios2 ^openmpi ^adios2-coeus@2.10.0
```
## Build and run Incompact3D with spack

### step 1: set the running scripts
go into example folder and copy the existed script as input.i3d.
This the the example for Pipe-Flow benchmark
```
cd Incompact3d/examples/Pipe-Flow
cp input_DNS_Re1000_LR.i3d input.i3d
```

### step 2: Bulid environment
```
spack load hermes@master
spack load incompact3D@coeus
spack load openmpi
export PATH=export PATH=/incompact3D/bin:$PATH
```


### step 3: set up the jarvis packages
```
jarvis ppl create incompact3d
jarvis ppl append Incompact3d example_location=/path/to/incompact3D-coeus engine=bp5 nprocs=16 ppn=16 benchmarks=Pipe-Flow
jarvis ppl env build

```

### step 4: Run with jarvis
```
jarvis ppl run
```


## Run the incompact3D with hermes

### step 1: set the running scripts
go into example folder and copy the existed script as input.i3d.
This the the example for Pipe-Flow benchmark
```
cd Incompact3d/examples/Pipe-Flow
cp input_DNS_Re1000_LR.i3d input.i3d
```

### step 2: Bulid environment
```
spack load hermes@master
spack load incompact3D@coeus
spack load openmpi
export PATH=export PATH=/incompact3D/bin:$PATH
export PATH=~/coeus-adapter/build/bin:$PATH
export LD_LIBRARY_PATH=~/coeus-adapter/build/bin:LD_LIBRARY_PATH
```

### step 3: set up the jarvis packages
```
jarvis ppl create incompact3d
jarvis ppl append hermes_run provider=sockets
jarvis ppl append Incompact3d example_location=/path/to/incompact3D-coeus engine=hermes nprocs=16 ppn=16 benchmarks=Pipe-Flow
jarvis ppl env build
```

### step 4: Run with jarvis
```
jarvis ppl run
```