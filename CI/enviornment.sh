#!/bin/bash

module unload adios
mdoule unload hdf5/parallel
module unload mpich

module load mpich
module load hdf5/parallel
module load adios