lammps is a molecule dynamic application.

# Install LAMMPS
use spack to install lammps
```
spack install lammps^adios2@2.9.0^mpi
```

# Download LAMMPS Example scripts
download the lammps script example
```
git clone https://github.com/simongravelle/lammps-input-files.git
```

# Set up the Environment and Run the LAMMPS
choose a example in lammps-input-files, for example, 2D-lennard-jones-fluid.
In 2D-lennard-jones-fluid folder, run the following command
```
spack load lammps
lmp -in input.lammps
```
# Run the LAMMPS with ADIOS
In 2D-lennard-jones-fluid folder/input.lammps, change the following code:
```
thermo 1000
dump mydmp all atom 1000 dump.lammpstrj
run 20000
```
to
```
thermo 1000
dump mydmp all atom/adios 1000 dump.lammpstrj
run 20000
```
then run the LAMMPS command, LAMMPS will automatically generate a adios configure file 
```
lmp -in input.lammps
```

# LAMMPS With Hermes

## 1. Setup Environment

Create the environment variables needed by Hermes + LAMMPS
```bash
spack load hermes@master
spack load lammps
spack load mpich
export PATH=/coeus-adapter/build/bin/:$PATH
export LD_LIBRARY_PATH=/coeus-adapter/build/bin/:$LD_LIBRARY_PATH

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
jarvis pipeline create lammps
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
jarvis pipeline append lammps ppn=?? nprocs=?? script_location=/the/location/of/script/folder engine=hermes
```

Jarvis also can support LAMMPS without hermes engine(only adios)
```bash
jarvis pipeline append hermes_run --sleep=10 --provider=sockets
jarvis pipeline append lammps ppn=?? nprocs=?? script_location=/the/location/of/script/folder engine=BP5
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
