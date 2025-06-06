
This is the post-processing to read file with adios2 bp5 from incompact3D examples.

### how to install
Installing the coeus-adapter will also generate an executable file named inCompact3D_analysis

### Jarvis(ADIOS2)

step 1: Build environment
```

spack load incompact3D@coeus
spack load openmpi
export PATH=/incompact3D/bin:$PATH
```
step 3: add jarvis repo
```
jarvis repo add coeus_adapter/test/jarvis/jarvis_coeus
```
step 4: Set up the jarvis packages
```
jarvis ppl create incompact3d
jarvis ppl append Incompact3d example_location=/path/to/incompact3D-coeus engine=bp5 nprocs=16 ppn=16 benchmarks=Pipe-Flow
jarvis ppl env build

```

step 5: Run with jarvis
```
jarvis ppl run
```