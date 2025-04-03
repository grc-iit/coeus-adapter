#!/bin/bash

# Define variables
L_values=(134 170 214 268 340 428 540 680 848)
nprocs_values=(1 2 4 8 16 32 64 128 256)
location="/path/to/storage"
report="/path/to/report/file"
for i in ${!L_values[@]}; do
  L=${L_values[$i]}
  nprocs=${nprocs_values[$i]}
  mkdir -p ${report}/${nprocs}process
  jarvis cd gray_scott
  jarvis pkg config adios2_gray_scott plotgap=1 out_file=${location}/${nprocs}/out1.bp ppn=20 nprocs=${nprocs} steps=40 L=$L checkpoint_output=${location}/${nprocs}/ckpt.bp
  jarvis ppl run > ${report}/${nprocs}process/gray_scott_1.txt

  jarvis pkg config adios2_gray_scott plotgap=1 out_file=${location}/${nprocs}/out2.bp ppn=20 nprocs=${nprocs} steps=40 L=$L checkpoint_output=${location}/${nprocs}/ckpt.bp
  jarvis ppl run > ${report}/${nprocs}process/gray_scott_2.txt

  jarvis cd hashing_compare
  jarvis pkg config hashing_compare ppn=20 nprocs=${nprocs} in_filename=${location}/${nprocs}/out1.bp out_filename=${location}/${nprocs}/out2.bp
  jarvis ppl run > ${report}/${nprocs}process/hashing.txt
  rm -r ${location}/${nprocs}


done