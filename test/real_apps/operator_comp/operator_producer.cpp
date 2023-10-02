#include <adios2.h>
#include <vector>
#include <cmath>
#include <chrono>
#include <iostream>
#include <mpi.h>
#include <fstream>

std::vector<double> produce_vector(int step) {
  return {step * 1.0, step * 2.0, step * 3.0};
}

template <typename T>
void print_vector(std::vector<T> vec){
  for(T obj : vec){
    std::cout << obj << " ";
  }
  std::cout << std::endl;
}

int main(int argc, char* argv[]) {
  MPI_Init(&argc, &argv);

  int rank, size;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  if (argc < 4) {
    if (rank == 0) {
      std::cerr << "Please provide the number of steps as an argument." << std::endl;
    }
    MPI_Finalize();
    return -1;
  }

  int N = std::stoi(argv[1]);
  std::string config_file = argv[2];
  std::string out_file = argv[3];

  adios2::ADIOS adios(config_file, MPI_COMM_WORLD);
  adios2::IO io = adios.DeclareIO("TestIO");
  auto var = io.DefineVariable<double>("vector", {size_t(size), 3}, {size_t(rank), 0}, {1, 3}, adios2::ConstantDims);

  adios2::Engine engine = io.Open(out_file, adios2::Mode::Write);

  double accumulated_time = 0.0;

  for (int step = 0; step < N; ++step) {
    std::vector<double> data = produce_vector(step);

    auto start = std::chrono::high_resolution_clock::now();

    engine.BeginStep();
    engine.Put(var, data.data());
    engine.EndStep();

    auto stop = std::chrono::high_resolution_clock::now();
    accumulated_time += std::chrono::duration<double, std::micro>(stop - start).count();
  }

  engine.Close();
  MPI_Barrier(MPI_COMM_WORLD);
  if(rank==0){
    std::cout << "\tPut done, time: " << accumulated_time << std::endl;
  }
  double total_time;
  MPI_Reduce(&accumulated_time, &total_time, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);

  MPI_Barrier(MPI_COMM_WORLD);

  if(rank == 0) {
    auto filename = "operator_producer_results.csv";
    std::string header = "Size,N,TotalTime\n";
    bool needHeader = false;

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
    outputFile << size << "," << N << "," << total_time << std::endl;
    outputFile.close();
  }

  MPI_Finalize();
  return 0;
}