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

    // Define variables for comparison
    std::vector<double> data1 = {0, 0, 0, 0, 0, 0};
    std::vector<double> data2 = {1, 1, 1, 1, 1, 1};
    std::vector<double> data3 = {2, 2, 2, 2, 2, 2};

    std::vector<double> data_get(6);

    const adios2::Dims shape = {2, 3};
    const adios2::Dims start = {0, 0};
    const adios2::Dims count = {2, 3};

    //adios2::Variable<double> var;

    // = io.DefineVariable<double>("myVar", shape, start, count);

    adios2::Engine reader = io.Open(file, adios2::Mode::Read);
    std::cout << "-- READER ENGINE INITIALIZED --" << std::endl;

    reader.BeginStep();
    reader.Get("myVar", data_get);
    assert(data1 == data_get);
    reader.EndStep();

    reader.BeginStep();
    reader.Get("myVar", data_get);
    assert(data2 == data_get);
    reader.EndStep();

    reader.BeginStep();
    reader.Get("myVar", data_get);
    assert(data3 == data_get);
    reader.EndStep();

    // Check metadata
    adios2::Variable<double> var_inquired = io.InquireVariable<double>("myVar");
    assert(var_inquired.Shape() == shape);
    assert(var_inquired.Start() == start);
    assert(var_inquired.Count() == count);

    std::pair<double, double> minmax_u = var_inquired.MinMax();

    std::cout << "Minimum value U: " << minmax_u.first << std::endl;
    std::cout << "Maximum value U: " << minmax_u.second << std::endl;


    reader.Close();

    std::cout << "All data arrays match!" << std::endl;
    std::cout << "Done" << std::endl;
    return 0;
}
