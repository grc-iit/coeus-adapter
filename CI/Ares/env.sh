#!/bin/bash

module unload adios2/2.9.0-mmkelnu
module unload hermes/master-rd5lvgk

module load adios2/2.9.0-mmkelnu
module load hermes/master-rd5lvgk

export LD_LIBRARY_PATH=/tmp/tmp.R9H27CdBpP/build/bin/:$LD_LIBRARY_PATH