Gray-Scott is a 3D 7-Point stencil code

# Installation

Since gray_scott is installed along with the coeus-adapter, these steps can be skipped.

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

We must make Jarvis aware of all environment variables needed to execute applications in the pipeline.
```bash
jarvis pipeline env build
```

## 4. Add pkgs to the Pipeline

Create a Jarvis pipeline with Gray Scott
```bash
jarvis pipeline append gray_scott
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
module load hermes/master-feow7up adios2/2.9.0-mmkelnu
# export GRAY_SCOTT_PATH=${HOME}/adiosvm/Tutorial/gs-mpiio/build
export PATH="${GRAY_SCOTT_PATH}:$PATH"
```

## 2. Create a Resource Graph

If you haven't already, create a resource graph. This only needs to be done
once throughout the lifetime of Jarvis. No need to repeat if you have already
done this for a different pipeline.

If you are running distributed tests, set path to the hostfile you are  using.
```bash
jarvis hostfile set /path/to/hostfile.txt
```

Next, collect the resources from each of those pkgs. Walkthrough will give
a command line tutorial on how to build the hostfile.
```bash
jarvis resource-graph build +walkthrough
```

## 3. Create a Pipeline

The Jarvis pipeline will store all configuration data needed by Hermes
and Gray Scott.

```bash
jarvis pipeline create gs-hermes
```

## 3. Save Environment

Store the current environment in the pipeline.
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


## 7. example to set the jarvis pipeline and derived variables:
```
jarvis pipeline create gray_scott
module load orangefs/2.10
module load openmpi
spack load hermes@master
export PATH=~/coeus/derived/coeus-adapter/build/bin/:$PATH
export LD_LIBRARY_PATH=~/coeus/derived/coeus-adapter/build/bin/:$LD_LIBRARY_PATH
jarvis pipeline env build
jarvis pipeline append hermes_run --sleep=10 --provider=sockets
jarvis pipeline append adios2_gray_scott engine=hermes_derived ppn=8 nprocs=16
jarvis ppl run

```
