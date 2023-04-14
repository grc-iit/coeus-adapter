//
// Created by lukemartinlogan on 3/28/23.
//

#include "coeus/hermes_engine.h"

int main() {
  adios2::ADIOS adios;

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

  // Write to file
  adios2::Engine writer = io.Open(file, adios2::Mode::Write);
  writer.Put(var, data.data());
  writer.Close();

  // Read from file
  adios2::Engine reader = io.Open(file, adios2::Mode::Read);
  reader.Get(var, data.data());
  reader.Close();

  std::cout << "Done" << std::endl;

  return 0;
}
