#!/bin/bash

# Define variables
io_time=(2 4 8 16 32 64 128 256 512)
nprocs_values=(1 2 4 8 16 32 64 128 256)
location="/mnt/ssd/hxu40/"
report="/path/to/report/file"
for i in ${!io_time[@]}; do
  nprocs=${nprocs_values[$i]}
  jarvis cd incompact3d
  # First configuration and run
  mkdir -p ${location}/${nprocs}process

  # Second configuration and run
  jarvis pkg config InCompact3D out_file=${location}/${nprocs}process/out2.bp ppn=20 nprocs=$nprocs
  jarvis pkg config InCompact3D_post in_filename=${location}/${nprocs}process/out2.bp ppn=20 nprocs=$nprocs out_filename=${location}/${nprocs}process/copy2.bp
  jarvis ppl run > ${report}/${nprocs}process/result.txt


done