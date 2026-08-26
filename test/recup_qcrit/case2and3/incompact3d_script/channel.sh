#!/bin/bash

# Define variables

steps=(5)
nprocs_values=(128)
location="/mnt/ssd/hxu40/orangefs/channel_output"
report="/mnt/common/hxu40/reports/time/incompact3d/hermes/mdh"
jarvis cd incompact3d_hermes
mkdir -p ${report}
for i in ${!steps[@]}; do
  nprocs=${nprocs_values[$i]}
  mkdir -p ${location}/${nprocs}process
  jarvis ppl kill
  jarvis pkg config InCompact3D output_location=${location}/${nprocs}process/ ppn=20 nprocs=$nprocs total_step=${steps[$i]} benchmarks=mdh
  jarvis ppl run  >> "${report}/${nprocs}process.run.log" 2>&1
  
done


report2="/mnt/common/hxu40/reports/time/incompact3d/mdh"
jarvis cd incompact3d
mkdir -p ${report2}
for i in ${!steps[@]}; do
  nprocs=${nprocs_values[$i]}
  # First configuration and run
  mkdir -p ${location}/${nprocs}process
  # Second configuration and run
  jarvis pkg config InCompact3D output_location=${location}/${nprocs}process/ ppn=20 nprocs=$nprocs total_step=${steps[$i]} benchmarks=mdh
  jarvis ppl run  >> "${report2}/${nprocs}process.run.log" 2>&1
  jarvis ppl kill
done
