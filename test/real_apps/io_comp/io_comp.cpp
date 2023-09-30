//
// Created by jaime on 9/27/2023.
//

#include <iostream>
#include <vector>
#include <chrono>
#include <fstream>
#include <string>
#include <sstream>
#include <mpi.h>
#include <adios2.h>

int main(int argc, char *argv[]) {
  int rank, size;
  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  if(argc != 3) {
    if(rank == 0) {
      std::cout << "Usage: " << argv[0] << " <N> <B>" << std::endl;
    }
    MPI_Finalize();
    return 1;
  }

  const int N = std::stoi(argv[1]);
  const size_t B = std::stoul(argv[2]);

  std::vector<char> data(B, rank);

  adios2::ADIOS adios("io_comp.xml", MPI_COMM_WORLD);
  adios2::IO io = adios.DeclareIO("TestIO");
  io.SetEngine("BPFile");

  auto variable = io.DefineVariable<char>("data", {size_t(size), B}, {size_t(rank), 0}, {1, B});

  auto engine = io.Open("data.bp", adios2::Mode::Write);

  MPI_Barrier(MPI_COMM_WORLD);
  double localPutTime = 0.0;
  for(int i = 0; i < N; ++i) {
    engine.BeginStep();
    auto startPut = std::chrono::high_resolution_clock::now();
    engine.Put(variable, data.data());
    auto endPut = std::chrono::high_resolution_clock::now();
    localPutTime += std::chrono::duration<double>(startPut - endPut).count();
    engine.EndStep();
  }
  MPI_Barrier(MPI_COMM_WORLD);

  engine.Close();

  adios2::IO readIO = adios.DeclareIO("ReadIO");
  auto readEngine = readIO.Open("data.bp", adios2::Mode::Read);
  adios2::Variable<char> readVariable = readIO.InquireVariable<char>("data");

  MPI_Barrier(MPI_COMM_WORLD);
  double localGetTime = 0.0;
  auto startGet = std::chrono::high_resolution_clock::now();
  for(int i = 0; i < N; ++i) {
    readEngine.BeginStep();
    auto startGet = std::chrono::high_resolution_clock::now();
    readEngine.Get(readVariable, data.data());
    auto endGet = std::chrono::high_resolution_clock::now();
    localGetTime += std::chrono::duration<double>(startGet - endGet).count();
    readEngine.EndStep();
  }
  auto endGet = std::chrono::high_resolution_clock::now();
  MPI_Barrier(MPI_COMM_WORLD);

  readEngine.Close();

  double globalPutTime, globalGetTime;
  MPI_Reduce(&localPutTime, &globalPutTime, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
  MPI_Reduce(&localGetTime, &globalGetTime, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);

  if(rank == 0) {
    globalPutTime /= (size*N);  // Calculate average
    globalGetTime /= (size*N);  // Calculate average
    std::ofstream outputFile("io_comp_results.csv", std::ios_base::app); // Append to the file
    outputFile << size << "," << B << "," << N << "," << globalPutTime << "," << globalGetTime << std::endl;
    outputFile.close();
  }

  MPI_Finalize();
  return 0;
}
