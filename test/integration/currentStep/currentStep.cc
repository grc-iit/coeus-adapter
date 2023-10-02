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

#include "coeus/HermesEngine.h"

namespace hapi = hermes::api;

int main() {

  char* ld_library_path = std::getenv("LD_LIBRARY_PATH");
  if (ld_library_path != nullptr) {
    std::cout << "LD_LIBRARY_PATH: " << ld_library_path << std::endl;
  } else {
    std::cout << "LD_LIBRARY_PATH is not set." << std::endl;
  }

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
  std::vector<double> data = {1, 2, 3, 4, 5, 6};
  const adios2::Dims shape = {2, 3};
  const adios2::Dims start = {0, 0};
  const adios2::Dims count = {2, 3};
  adios2::Variable<double> var = io.DefineVariable<double>(
    "myVar", shape, start, count);

  adios2::Engine writer = io.Open(file, adios2::Mode::Read);

  if(writer) { std::cout << "mEngine is defined" << std::endl; }
  else { std::cout << "mEngine is not defined" << std::endl; }

  std::cout << adios2::ToString(writer) << std::endl;
  writer.BeginStep();
  auto step = writer.CurrentStep();
  assert(step == 1);
  writer.EndStep();

  step = 0;

  return 0;
}
