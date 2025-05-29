
This is the post-processing to read file with adios2 bp5 from incompact3D examples.

### how to install
Install the coeus-adapter and a execute file called inCompact3D_analysis will also be generated. 

### how to use

```
jarvis ppl create incompact3D_post
jarvis ppl append InCompact3D_post file_location=/path/to/data.bp5 nprocs=16 ppn=16 engine=bp5
export PATH=~/coeus-adapter/build/bin/:$PATH
spack load hemres@master
spack load openmpi
jarvis ppl env build
jarvis ppl run
```