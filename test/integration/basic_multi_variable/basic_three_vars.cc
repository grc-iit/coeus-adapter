/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Distributed under BSD 3-Clause license.                                   *
 * Copyright by the Illinois Institute of Technology.                        *
 * All rights reserved.                                                      *
 *                                                                           *
 * This file is part of Coeus-adapter. The full Coeus-adapter copyright      *
 * notice, including terms governing use, modification, and redistribution,  *
 * is contained in the COPYING file, which can be found at the top directory.*
 * If you do not have access to the file, you may request a copy             *
 * from scslab@iit.edu.                                                      *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include <cassert>
#include "coeus/HermesEngine.h"

namespace hapi = hermes::api;

int main() {
    adios2::ADIOS adios;
    // NOTE(llogan): LD_LIBRARY_PATH must point to the directory which contains
    // the libhermes_engine.so file.

    // The file we will be creating
    std::string file = "/tmp/myFile.bp";
    // We will be performing writes
    adios2::IO io = adios.DeclareIO("CoeusTest");
    // Tell ADIOS we will be using a custom plugin
    io.SetEngine("Plugin");
    // Specify which plugin to use
    adios2::Params params;
    // The name that will be passed to the engine
    // should be the file you're trying to open
    params["PluginName"] = file;
    // The name of the shared library in the CMakeLists.txt
    params["PluginLibrary"] = "hermes_engine";
    // Any other paramaters to the engine
    io.SetParameters(params);

    // Define variable
    std::vector<double> data1 = {0, 0, 0, 0, 0, 0};
    std::vector<double> data2 = {1, 1, 1, 1, 1, 1};
    std::vector<double> data3 = {2, 2, 2, 2, 2, 2};

    const adios2::Dims shape = {2, 3};
    const adios2::Dims start = {0, 0};
    const adios2::Dims count = {2, 3};

    adios2::Variable<double> var = io.DefineVariable<double>(
            "myVar", shape, start, count);

    // Write to file
    adios2::Engine writer = io.Open(file, adios2::Mode::Write);
    writer.BeginStep();
    writer.Put(var, data1.data());
    writer.EndStep();

    writer.BeginStep();
    writer.Put(var, data2.data());
    writer.EndStep();

    writer.BeginStep();
    writer.Put(var, data3.data());
    writer.EndStep();

    writer.Close();

    std::vector<double> data1_get(data1);
    std::vector<double> data2_get(data2);
    std::vector<double> data3_get(data3);

    // Read from file
    adios2::Engine reader = io.Open(file, adios2::Mode::Read);
    reader.BeginStep();
    reader.Get(var, data1_get.data());
    reader.EndStep();

    reader.BeginStep();
    reader.Get(var, data2_get.data());
    reader.EndStep();

    reader.BeginStep();
    reader.Get(var, data3_get.data());
    reader.EndStep();

    reader.Close();

    assert(data1 == data1_get);
    assert(data2 == data2_get);
    assert(data3 == data3_get);

    std::cout << "All data arrays match!" << std::endl;

    std::cout << "Done" << std::endl;

    return 0;
}
