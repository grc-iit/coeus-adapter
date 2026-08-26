#!/bin/bash

# Define variables
L_values=(134 170 214 268 340 428 540 680 848)
nprocs_values=(1 2 4 8 16 32 64 128 256 )
location="/path/to/storage"
report="/path/to/report/file"
for i in ${!L_values[@]}; do
  L=${L_values[$i]}
  nprocs=${nprocs_values[$i]}
  jarvis cd gray_scott_hermes
  mkdir -p ${report}/${nprocs}process
  jarvis pkg config adios2_gray_scott plotgap=1 L=$L steps=40 out_file=${location}/${nprocs}/out1.bp nprocs=${nprocs} ppn=20 checkpoint_output=${location}/${nprocs}/ckpt.bp engine=hermes_derived db_path=benchmark_metadata.db
  jarvis pkg config adios2_gray_scott_2 plotgap=1 L=$L steps=40 out_file=${location}/${nprocs}/out2.bp nprocs=${nprocs} ppn=20 checkpoint_output=${location}/${nprocs}/ckpt.bp engine=hermes_derived db_path=benchmark_metadata.db
  jarvis pkg config adios2_gray_scott_3 plotgap=1 L=$L steps=40 out_file=${location}/${nprocs}/out3.bp nprocs=${nprocs} ppn=20 checkpoint_output=${location}/${nprocs}/ckpt.bp engine=hermes_derived db_path=benchmark_metadata.db
  jarvis ppl run > ${report}/${nprocs}process/case3.txt
  jarvis ppl clean
  jarvis ppl kill
  jarvis ppl update

done
