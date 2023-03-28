//
// Created by lukemartinlogan on 3/28/23.
//

#include "coeus/hermes_engine.h"

int main() {
  adios2::ADIOS adios;

  // We will be performing writes
  adios2::IO io = adios.DeclareIO("writer");
  // Tell ADIOS we will be using a custom plugin
  io.SetEngine("Plugin");
  // Specify which plugin to use
  adios2::Params params;
  // The name that you want ADIOS to use internally
  params["PluginName"] = "hermes_engine";
  // The name of the shared library in the CMakeLists.txt
  params["PluginLibrary"] = "hermes_engine";
  // Any other paramaters to the engine
  io.SetParameters(params);

  // TODO(llogan): actually try doing I/O here to see if the engine was found

  return 0;
}
