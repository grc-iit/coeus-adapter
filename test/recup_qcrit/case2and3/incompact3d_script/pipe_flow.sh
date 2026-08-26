#!/bin/bash

# Define variables

steps=(16 32 64 128 256 512 1024)
nprocs_values=(1 2 4 8 16 32 64)
location="/mnt/ssd/hxu40/orangefs/pipe_output"
report="/mnt/common/hxu40/reports/time/incompact3d/hermes/pipe_flow"
jarvis cd incompact3d_hermes
mkdir -p ${report}
for i in ${!steps[@]}; do
  nprocs=${nprocs_values[$i]}
  mkdir -p ${location}/${nprocs}process
  jarvis pkg config incompact3d output_location=${location}/${nprocs}process/ ppn=20 nprocs=$nprocs total_step=${steps[$i]} benchmarks=pipe_flow
  jarvis ppl run  >> "${report}/${nprocs}process.run.log" 2>&1
  jarvis ppl kill
done


report2="/mnt/common/hxu40/reports/time/incompact3d/pipe_flow"
jarvis cd incompact3d
mkdir -p ${report2}
for i in ${!steps[@]}; do
  nprocs=${nprocs_values[$i]}
  # First configuration and run
  mkdir -p ${location}/${nprocs}process
  # Second configuration and run
  jarvis pkg config incompact3d output_location=${location}/${nprocs}process/ ppn=20 nprocs=$nprocs total_step=${steps[$i]} benchmarks=pipe_flow
  jarvis ppl run  >> "${report2}/${nprocs}process.run.log" 2>&1
  jarvis ppl kill
done
