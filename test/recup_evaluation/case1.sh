#!/bin/bash

# Define variables

L_values=(134 170 214 268 340 428 540 680 848)
nprocs_values=(1 2 4 8 16 32 64 128 256)
location="/add/location"
report="/add/out_put_file"

# this is the jarvis pipeline establish, once

for i in ${!L_values[@]}; do
  L=${L_values[$i]}
  nprocs=${nprocs_values[$i]}
  jarvis cd gray_scott_bp5
  # First configuration and run
  mkdir -p ~/${report}/${nprocs}process
  jarvis pkg config adios2_gray_scott plotgap=1 out_file=/mnt/${location}/hxu40/ofs-mount/case1/${nprocs}process/out1.bp ppn=20 nprocs=$nprocs steps=40 L=$L checkpoint_output=/mnt/${location}/hxu40/ofs-mount/case1/ckpt.bp
  jarvis pkg config adios_hashing in_filename=/mnt/${location}/hxu40/ofs-mount/case1/${nprocs}process/out1.bp ppn=20 nprocs=$nprocs out_filename=/mnt/${location}/hxu40/ofs-mount/case1/${nprocs}process/copy1.bp
  jarvis ppl run >> ~/${report}/${nprocs}process/result1.txt


  # Second configuration and run
  jarvis pkg config adios2_gray_scott plotgap=1 out_file=/mnt/${location}/hxu40/ofs-mount/case1/${nprocs}process/out2.bp ppn=20 nprocs=$nprocs steps=40 L=$L checkpoint_output=/mnt/${location}/hxu40/ofs-mount/case1/ckpt.bp
  jarvis pkg config adios_hashing in_filename=/mnt/${location}/hxu40/ofs-mount/case1/${nprocs}process/out2.bp ppn=20 nprocs=$nprocs out_filename=/mnt/${location}/hxu40/ofs-mount/case1/${nprocs}process/copy2.bp
  jarvis ppl run >> ~/${report}/${nprocs}process/result2.txt


  # Hashing compare
  jarvis cd hashing_compare
  jarvis pkg config hashing_compare nprocs=$nprocs ppn=20 in_filename=/mnt/${location}/hxu40/ofs-mount/case1/${nprocs}process/copy1.bp out_filename=/mnt/${location}/hxu40/ofs-mount/case1/${nprocs}process/copy2.bp
  jarvis ppl run >> ~/${report}/${nprocs}process/result3.txt
  rm -r /mnt/${location}/hxu40/ofs-mount/case1/${nprocs}process

done