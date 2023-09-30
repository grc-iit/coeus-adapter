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

int main(int argc, char* argv[]) {
  MPI_Init(&argc, &argv);

  int rank, size;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  if (argc < 2) {
    if (rank == 0) {
      std::cerr << "Please provide the number of steps as an argument." << std::endl;
    }
    MPI_Finalize();
    return -1;
  }

  int N = std::stoi(argv[1]);  // Number of steps

  adios2::ADIOS adios("operator_comp.xml", MPI_COMM_WORLD);
  adios2::IO io = adios.DeclareIO("TestIO");
  adios2::Engine engine = io.Open("data.bp", adios2::Mode::Write);

  double accumulated_time = 0.0;

  for (int step = 0; step < N; ++step) {
    std::vector<double> data = produce_vector(step);
    auto var = io.DefineVariable<double>("vector", {3}, {0}, {3}, adios2::ConstantDims);

    auto start = std::chrono::high_resolution_clock::now();

    engine.BeginStep();
    engine.Put(var, data.data());
    engine.EndStep();

    auto stop = std::chrono::high_resolution_clock::now();
    accumulated_time += std::chrono::duration<double, std::micro>(stop - start).count();
  }

  engine.Close();

  double total_time;
  MPI_Reduce(&accumulated_time, &total_time, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);

  MPI_Barrier(MPI_COMM_WORLD);

  if (rank == 0) {
    std::ofstream outputFile("operator_consumer_results.csv", std::ios_base::app); // Append to the file
    outputFile << size << "," << N << "," << total_time << std::endl;
    outputFile.close();
  }

  MPI_Finalize();
  return 0;
}