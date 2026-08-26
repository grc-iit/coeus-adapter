#!/bin/bash

# Define variables

steps=(15 15)
nprocs_values=(64 128)
location="/mnt/ssd/hxu40/orangefs/tbl_output"
report="/mnt/common/hxu40/reports/time/incompact3d/hermes/tbl_Nz_64"
jarvis cd incompact3d_hermes
mkdir -p ${report}
for i in ${!steps[@]}; do
  nprocs=${nprocs_values[$i]}
  mkdir -p ${location}/${nprocs}process
  jarvis pkg config InCompact3D output_location=${location}/${nprocs}process/ ppn=20 nprocs=$nprocs total_step=${steps[$i]} benchmarks=tbl
  jarvis ppl run  >> "${report}/${nprocs}process.run.log" 2>&1
  jarvis ppl kill
done


report2="/mnt/common/hxu40/reports/time/incompact3d/tbl_Nz_64"
jarvis cd incompact3d
mkdir -p ${report2}
for i in ${!steps[@]}; do
  nprocs=${nprocs_values[$i]}
  # First configuration and run
  mkdir -p ${location}/${nprocs}process
  # Second configuration and run
  jarvis pkg config InCompact3D output_location=${location}/${nprocs}process/ ppn=20 nprocs=$nprocs total_step=${steps[$i]} benchmarks=tbl
  jarvis ppl run  >> "${report2}/${nprocs}process.run.log" 2>&1
  jarvis ppl kill
done
