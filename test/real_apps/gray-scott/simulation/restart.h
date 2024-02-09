#ifndef __RESTART_H__
#define __RESTART_H__

#include "../../gray-scott/simulation/gray-scott.h"
#include "../../gray-scott/simulation/settings.h"

#include <adios2.h>
#include <mpi.h>

void WriteCkpt(MPI_Comm comm, const int step, const Settings &settings,
               const GrayScott &sim, adios2::IO io);
int ReadRestart(MPI_Comm comm, const Settings &settings, GrayScott &sim,
                adios2::IO io);

#endif
