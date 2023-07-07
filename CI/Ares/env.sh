#!/bin/bash

module unload adios2/2.9.0-mmkelnu
module unload hermes/master-feow7up

module load adios2/2.9.0-mmkelnu
module load hermes/master-feow7up

export LD_LIBRARY_PATH=/tmp/tmp.5TUlmkfNlZ/build-remote/bin/:$LD_LIBRARY_PATH