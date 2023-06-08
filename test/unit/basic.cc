//
// Created by lukemartinlogan on 3/28/23.
//

#include "coeus/hermes_engine.h"

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
  std::vector<double> data = {1, 2, 3, 4, 5, 6};
  const adios2::Dims shape = {2, 3};
  const adios2::Dims start = {0, 0};
  const adios2::Dims count = {2, 3};
  adios2::Variable<double> var = io.DefineVariable<double>(
    "myVar", shape, start, count);

  // Write to file
  coeus::HermesEngine* writer = EngineCreate(io, file, adios2::Mode::Write); // Needs an extra param
  //hapi::Hermes::HermesEngine writer = EngineCreate(io, file, adios2::Mode::Write); // Needs an extra param
  auto bkt1 = writer->GetBucket("myVar");
  size_t blob_size = KILOBYTES(4);
  hapi::Context ctx;
  hermes::BlobId blob_id1;
  hermes::Blob blob(blob_size);
  memset(blob.data(), data.data(), blob_size);
  bkt1.Put(var, blob, blob_id1, ctx);

  //for(size_t i = 0; i < blob_size; ++i){
    //  memset(blob.data(), i, blob_size);
     // bkt1.Put("myVar", blob, blob_id1, ctx);
  //}

/*
  // Write to file
  adios2::Engine writer = io.Open(file, adios2::Mode::Write);
  writer.Put(var, data.data());
  writer.Close();
*/

  // Read from file
  coeus::HermesEngine* reader = EngineCreate(io, file, adios2::Mode::Read); // Needs an extra param
  // hapi::Hermes::HermesEngine reader = EngineCreate(io, file, adios2::Mode::Read); // Needs an extra param
  auto bkt2 = reader->GetBucket("myVar");
  hapi::Context ctx2;
  hermes::Blob blob2:
  bkt2.GetBlob("myVar", blob_id);
  REQUIRE(!blob_id.IsNull());
  bkt2.Get(blob_id, blob2, ctx2);

  /*
  // Read from file
  adios2::Engine reader = io.Open(file, adios2::Mode::Read);
  reader.Get(var, data.data());
  reader.Close();
*/
  std::cout << "Done" << std::endl;

  return 0;
}
