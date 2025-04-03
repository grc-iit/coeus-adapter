#!/bin/bash

# Define variables
steps=(2 4 8 16 32 64 128 256 512)
nprocs_values=(1 2 4 8 16 32 64 128 256)
location="/path/to/storage"
report="/path/to/report/file"
for i in ${!steps[@]}; do
  step=${steps[$i]}
  nprocs=${nprocs_values[$i]}
  jarvis cd gray_scott_bp5
  # First configuration and run
  mkdir -p ${report}/${nprocs}process
  jarvis pkg config adios2_gray_scott plotgap=1 out_file=${location}/${nprocs}process/out1.bp ppn=20 nprocs=$nprocs steps=$step L=512
  jarvis pkg config adios_hashing in_filename=${location}/out1.bp ppn=20 nprocs=$nprocs out_filename=${location}/${nprocs}process/copy1.bp
  jarvis ppl run > ${report}/${nprocs}process/gray_scott_1.txt


  # Second configuration and run
  jarvis pkg config adios2_gray_scott plotgap=1 out_file=${location}/${nprocs}process/out2.bp ppn=20 nprocs=$nprocs steps=$step L=512
  jarvis pkg config adios_hashing in_filename=${location}/${nprocs}process/out2.bp ppn=20 nprocs=$nprocs out_filename=${location}/${nprocs}process/copy2.bp
  jarvis ppl run > ${report}/${nprocs}process/gray_scott_2.txt


  # Hashing compare
  jarvis cd hashing_compare
  jarvis pkg config hashing_compare ppn=20 nprocs=$nprocs in_filename=${location}/${nprocs}process/copy1.bp out_filename=${location}/${nprocs}process/copy2.bp
  jarvis ppl run > ${report}/${nprocs}process/compare.txt
  rm -r ${location}/${nprocs}process

done