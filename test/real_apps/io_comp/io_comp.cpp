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

//template <typename T>
//void print_vector(std::vector<T> vec){
//  for(T obj : vec){
//    std::cout << obj << " ";
//  }
//  std::cout << std::endl;
//}
//
//template <typename T>
//void print_meta(int rank, int size, adios2::Variable<T> var){
//  for(int i = 0; i < size; i++){
//    if(i==rank){
//      std::cout << rank << std::endl;
//      std::cout << var.Name() << std::endl;
//      print_vector(var.Shape());
//      print_vector(var.Start());
//      print_vector(var.Count());
//    }
//    MPI_Barrier(MPI_COMM_WORLD);
//  }
//}

int main(int argc, char *argv[]) {
  int rank, size;
  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  if(argc < 5) {
    if(rank == 0) {
      std::cout << "Usage: " << argv[0] << " <N> <B> " << std::endl;
    }
    MPI_Finalize();
    return 1;
  }

  const int N = std::stoi(argv[1]);
  const size_t B = std::stoul(argv[2]);
  std::string config_path = argv[3];
  std::string out_file = argv[4];

  if(rank==0) {
    std::cout << "Running I/O comparison with " << N << " steps, " <<
              B << " bytes per step, and " << size << " processes." << std::endl;
  }
  double localGetTime = 0.0;
  double localPutTime = 0.0;
  std::string engine_name;
  {
    adios2::ADIOS adios(config_path, MPI_COMM_WORLD);
    adios2::IO io = adios.DeclareIO("TestIO");

    std::vector<char> data(B, rank);
    auto variable = io.DefineVariable<char>("data", {size_t(size), B}, {size_t(rank), 0}, {1, B});

    auto engine = io.Open(out_file, adios2::Mode::Write);
    engine_name = engine.Name();
    MPI_Barrier(MPI_COMM_WORLD);
    for (int i = 0; i < N; ++i) {
      engine.BeginStep();

      auto startPut = std::chrono::high_resolution_clock::now();
      engine.Put<char>(variable, data.data());
      auto endPut = std::chrono::high_resolution_clock::now();
      localPutTime += std::chrono::duration<double>(endPut - startPut).count();

      engine.EndStep();
    }
    engine.Close();

    MPI_Barrier(MPI_COMM_WORLD);
    if (rank == 0) {
      std::cout << "\tPut done, time: " << localPutTime << std::endl;
    }
  }

  {
    adios2::ADIOS adios(config_path, MPI_COMM_WORLD);
    adios2::IO io = adios.DeclareIO("TestIO");
    auto readEngine = io.Open(out_file, adios2::Mode::Read);

    std::vector<char> data;

    MPI_Barrier(MPI_COMM_WORLD);

    if (rank == 0) std::cout << "BeginStep" << std::endl;

    while (readEngine.BeginStep() == adios2::StepStatus::OK) {
      adios2::Variable<char> readVariable = io.InquireVariable<char>("data");

      auto startGet = std::chrono::high_resolution_clock::now();
      readEngine.Get<char>(readVariable, data);
      auto endGet = std::chrono::high_resolution_clock::now();
      localGetTime += std::chrono::duration<double>(endGet - startGet).count();

      readEngine.EndStep();
    }
    readEngine.Close();

    MPI_Barrier(MPI_COMM_WORLD);
    if (rank == 0) {
      std::cout << "\tGet done, time: " << localGetTime << std::endl;
    }
  }

  double globalPutTime, globalGetTime;
  MPI_Reduce(&localPutTime, &globalPutTime, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
  MPI_Reduce(&localGetTime, &globalGetTime, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);

  if(rank == 0) {
    std::string header = "Size,B,N,GlobalPutTime,GlobalGetTime\n";
    bool needHeader = false;

    auto filename = "io_comp_results.csv";
    std::cout << "Writing results to " << filename << std::endl;
    // Check if the file is empty or doesn't exist
    std::ifstream checkFile(filename);
    if (!checkFile.good() || checkFile.peek() == std::ifstream::traits_type::eof()) {
      needHeader = true;
    }
    checkFile.close();

    // Open the file for appending
    std::ofstream outputFile(filename, std::ios_base::app);

    // Write the header if needed
    if (needHeader) {
      outputFile << header;
    }

    // Append the results
    outputFile << size << "," << B << "," << N << "," << globalPutTime << "," << globalGetTime << std::endl;
    outputFile.close();
  }

  MPI_Finalize();
  return 0;
}
