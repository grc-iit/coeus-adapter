#!/bin/bash

# Define variables

steps=(20 20)
nprocs_values=(16 32)
location="/mnt/ssd/hxu40/orangefs/abl_output"
report="/mnt/common/hxu40/reports/time/incompact3d/hermes/abl"
jarvis cd incompact3d_hermes
mkdir -p ${report}
for i in ${!steps[@]}; do
  nprocs=${nprocs_values[$i]}
  # First configuration and run
  mkdir -p ${location}/${nprocs}process
  jarvis ppl kill
  # Second configuration and run
  jarvis pkg config InCompact3D output_location=${location}/${nprocs}process/ ppn=20 nprocs=$nprocs total_step=${steps[$i]} benchmarks=abl
  jarvis ppl run  >> "${report}/${nprocs}process.run.log" 2>&1
  
done


report2="/mnt/common/hxu40/reports/time/incompact3d/abl"
jarvis cd incompact3d
mkdir -p ${report2}
for i in ${!steps[@]}; do
  nprocs=${nprocs_values[$i]}
  # First configuration and run
  mkdir -p ${location}/${nprocs}process
  # Second configuration and run
  jarvis pkg config InCompact3D output_location=${location}/${nprocs}process/ ppn=20 nprocs=$nprocs total_step=${steps[$i]} benchmarks=abl
  jarvis ppl run  >> "${report2}/${nprocs}process.run.log" 2>&1
  jarvis ppl kill
done
