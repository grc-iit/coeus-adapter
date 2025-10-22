#!/bin/bash

# Define variables
steps=(30 60 120 240 480 960 1920 3840)
nprocs_values=(1 2 4 8 16 32 64 128)
location="/mnt/ssd/hxu40/orangefs/cylinder_output"
report="/path/to/report/file"
spack load incompact3D@coeus
for i in ${!steps[@]}; do
  nprocs=${nprocs_values[$i]}
  jarvis cd incompact3d
  # First configuration and run
  mkdir -p ${location}/${nprocs}process
  # Second configuration and run
  jarvis pkg config incompact3d output_location=${location}/${nprocs}process/ ppn=20 nprocs=$nprocs total_step=${steps[$i]}
  cd ${location}/${nprocs}process/
  mpirun -np ${nprocs} --hostfile /mnt/common/hxu40/server_list/server.list   --mca pml ob1 --mca btl tcp,self   --mca osc ^ucx -mca btl_tcp_if_include eno1   --mca oob_tcp_if_include eno1 /mnt/common/hxu40/spack/opt/spack/linux-ubuntu22.04-skylake_avx512/gcc-11.4.0/incompact3D-coeus-23xkxsu2rwpg5h4nm5ry72b5f7cltgmo/bin/xcompact3d >> run.log 2>&1
done