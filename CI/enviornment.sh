#!/bin/bash

if [ -f /.dockerenv ]; then
     source /etc/profile
else
  if [ "$(hostname)" = "ares.ares.local" ]; then
       module unload adios
       mdoule unload hdf5/parallel
       module unload mpich

       module load mpich
       module load hdf5/parallel
       module load adios
  else
      echo "Neither ares nor docker"
  fi

fi

