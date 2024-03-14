#ifndef __WRITER_H__
#define __WRITER_H__

#include <adios2.h>
#include <mpi.h>

#include "../../gray-scott/simulation/gray-scott.h"
#include "../../gray-scott/simulation/settings.h"

class Writer
{
public:
    Writer(const Settings &settings, const GrayScott &sim, adios2::IO io, bool derived);
    void open(const std::string &fname, bool append);
    void write(int step, const GrayScott &sim, int rank);
    void close();

protected:
    Settings settings;

    adios2::IO io;
    adios2::Engine writer;
    adios2::Variable<double> var_u;
    adios2::Variable<double> var_v;
    adios2::Variable<int> var_step;
    auto PDFU;
    auto PDFV;
};

#endif
