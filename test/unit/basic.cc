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
  hapi::Hermes::HermesEngine writer = EngineCreate(io, file, adios2::Mode::Write ); // Falta un parametro
  auto bkt1 = writer->GetBucket("myVar"); // esto no se si esta bien asi (si no ponemos "myVar"). El writer seria el objeto hermes
  size_t blob_size = KILOBYTES(4);
  hapi::Hermes::api::Context ctx;
  hapi::Hermes::BlobId blob_id1;
  hapi::Hermes::Blob blob(blob_size);
  memset(blob.data(), data.data(), blob_size); // no se como se relaciona con las io de adios
  bkt1.Put(var, blob, blob_id1, ctx); // no se si pasar la var de adios es valido

  /*
  adios2::Engine writer = io.Open(file, adios2::Mode::Write);
  writer.Put(var, data.data());
  writer.Close();*/

  // Read from file
  hapi::Hermes::HermesEngine reader = EngineCreate(io, file, adios2::Mode::Read ); //Falta un parametro que no se que es
  auto bkt2 = reader->GetBucket(var.name); // esto no se si esta bien asi (si no ponemos "myVar"). El writer seria el objeto hermes
  hapi::Hermes::api::Context ctx2;
  hapi::Hermes::Blob blob2;
  bkt2.GetBlobId(var.name, blob_id);
  REQUIRE(!blob_id.IsNull());
  bkt2.Get(blob_id, blob2, ctx2);

    /*
    adios2::Engine reader = io.Open(file, adios2::Mode::Read);
    reader.Get(var, data.data());
    reader.Close();*/
  using HermesPtr = std::shared_ptr<hapi::Hermes>;


  hapi::Bucket bkt("shared bucket", HermesPtr); // create bucket ?withfilename?
  constexpr int bytes_per_blob = KILOBYTES(3);

  hapi::Blob blobPut(bytes_per_blob, data.data()); //aqui el hace con x, creo que podriamos rellenarlo con la data correspondiente?
  status = bkt.Put("varName", blobPut);
  Assert(status.Succeeded());

  hapi::Blob blobGet;
  size_t blob_size = bucket.Get(blob_name, blobGet);

  Assert(get_result == put_data);

  bkt.Destroy();

  std::cout << "Done" << std::endl;

  return 0;
}
