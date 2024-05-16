Gray-Scott is a 3D 7-Point stencil code

# Installation
Since gray_scott is installed along with the coeus-adapter, these steps can be skipped.
```bash
git clone https://github.com/pnorbert/adiosvm
pushd adiosvm/Tutorial/gs-mpiio
mkdir build
pushd build
cmake ../ -DCMAKE_BUILD_TYPE=Release
make -j8
export GRAY_SCOTT_PATH=`pwd`
popd
popd
```

# Gray Scott

## 1. Setup Environment

Create the environment variables needed by Gray Scott
```bash
spack load mpi
export PATH="${COEUS_Adapter/build/bin}:$PATH"
```````````



## 2. Create a Pipeline

The Jarvis pipeline will store all configuration data needed by Gray Scott.

```bash
jarvis pipeline create gray-scott-test
```

## 3. Save Environment

Store the current environment in the pipeline.
```bash
jarvis pipeline env build
```

## 4. Add pkgs to the Pipeline

Create a Jarvis pipeline with Gray Scott
```bash
jarvis pipeline append adios2_gray_scott

```

## 5. Run Experiment

Run the experiment
```bash
jarvis pipeline run
```

## 6. Clean Data

Clean data produced by Gray Scott
```bash
jarvis pipeline clean
```

# Gray Scott With Hermes

## 1. Setup Environment

Create the environment variables needed by Hermes + Gray Scott
```bash
# On personal
spack install hermes@master adios2
spack load hermes adios2
# On Ares
spack load hermes@master
# export GRAY_SCOTT_PATH=$/coeus_adapter/build/bin
export PATH="${COEUS_Adapter/build/bin}:$PATH"
```


## 2. Create a Pipeline

The Jarvis pipeline will store all configuration data needed by Hermes
and Gray Scott.

```bash
jarvis pipeline create gs-hermes
```

## 3. Save Environment

We must make Jarvis aware of all environment variables needed to execute applications in the pipeline.

```bash
jarvis pipeline env build
```

## 4. Add pkgs to the Pipeline

Create a Jarvis pipeline with Hermes, the Hermes MPI-IO interceptor,
and gray-scott
```bash
jarvis pipeline append hermes_run --sleep=10 --provider=sockets
jarvis pipeline append adios2_gray_scott engine=hermes 
```

For derived variable with adios2 in hermes:
```bash
jarvis pipeline append hermes_run --sleep=10 --provider=sockets
jarvis pipeline append adios2_gray_scott engine=hermes_derived
```

## 5. Run the Experiment

Run the experiment
```bash
jarvis pipeline run
```

## 6. Clean Data

To clean data produced by Hermes + Gray-Scott:
```bash
jarvis pipeline clean
```
