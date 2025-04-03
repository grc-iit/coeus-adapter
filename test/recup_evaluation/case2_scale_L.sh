#!/bin/bash

# Define variables
steps=(2 4 8 16 32 64 128 256 512)
nprocs_values=(1 2 4 8 16 32 64 128 256)
location="/path/to/storage"
report="/path/to/report/file"
for i in ${!nprocs_values[@]}; do
  step=${steps[$i]}
  nprocs=${nprocs_values[$i]}
  mkdir -p ~/${report}/${nprocs}process
  jarvis cd gray_scott
  jarvis pkg config adios2_gray_scott plotgap=1 out_file=${location}/${nprocs}/out1.bp ppn=20 nprocs=${nprocs} checkpoint_output=${location}/${nprocs}/ckpt.bp steps=$step L=512
  jarvis ppl run > ${report}/${nprocs}process/result.txt

  jarvis pkg config adios2_gray_scott plotgap=1 out_file=${location}/${nprocs}/out2.bp ppn=20 nprocs=${nprocs} checkpoint_output=${location}/${nprocs}/ckpt.bp steps=$step L=512
  jarvis ppl run &> ${report}/${nprocs}process/result1.txt

  jarvis cd hashing_compare
  jarvis pkg config hashing_compare ppn=20 nprocs=${nprocs} in_filename=${location}/${nprocs}/out1.bp out_filename=${location}/${nprocs}/out2.bp
  jarvis ppl run &> ${report}/${nprocs}process/result2.txt
  rm -r ${location}/${nprocs}


done