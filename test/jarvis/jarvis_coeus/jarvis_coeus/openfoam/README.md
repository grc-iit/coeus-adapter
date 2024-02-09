Openfoam is a fully dynamic application.

# Install openfoam
use spack to install openfoam
```
spack install openfoam@2206^adios2@2.9.0^mpi
```

# Download ADIOSFOAM
download the adiosfoam, a modified adiosfoam version is available to support
```
git clone https://github.com/hxu65/adiosfoam.git
```

# Set up the Environment and Run the Openfoam


```
# Run the Openfoam with ADIOS
In adiosfoam/tutorials/finiteArea/surfactantFoam/planeTransport
create a adios2.xml configuration file
```
spack load adios
./Allrun
```

# OpenFoam With Hermes

## 1. Setup Environment

Create the environment variables needed by Openfoam and Hermes
```bash
spack load hermes@master
spack load Openfoam
spack load mpich
export PATH=/coeus-adapter/build/bin/:$PATH
export LD_LIBRARY_PATH=/coeus-adapter/build/bin/:$LD_LIBRARY_PATH

```

## 2. Install Jarvis and set up Jarvis
Please refer this website for more information about Jarvis.  
https://grc.iit.edu/docs/jarvis/jarvis-cd/index

## 3. Create a Pipeline

The Jarvis pipeline will store all configuration data needed by Hermes
and Gray Scott.

```bash
jarvis pipeline create openfoam
```

## 3. Save Environment

Store the current environment in the pipeline.
```bash
jarvis pipeline env build
```

## 4. Add pkgs to the Pipeline

Create a Jarvis pipeline with Hermes, the Hermes MPI-IO interceptor, and OpenFOAM
```bash
jarvis pipeline append hermes_run --sleep=10 --provider=sockets
jarvis pipeline append openfoam ppn=4 nprocs=1 script_location=/adiosfoam/tutorials/finiteArea/surfactantFoam/planeTransport/ engine=hermes
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
