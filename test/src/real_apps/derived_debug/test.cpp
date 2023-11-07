#include <iostream>
#include <vector>
#include <chrono>
#include <fstream>
#include <string>
#include <sstream>
#include <mpi.h>
#include <adios2.h>
#include <random>
#include <unistd.h>

void mpi_sleep(int time, int rank, std::string reason){
  MPI_Barrier(MPI_COMM_WORLD);
  if(rank == 0) std::cout << reason << std::endl;
  sleep(time);
  if(rank == 0) std::cout << "Done " << reason << std::endl;
  MPI_Barrier(MPI_COMM_WORLD);
}

std::vector<float> generateRandomVector(std::size_t size, int rank) {
  // Use a combination of current time and the process rank as seed

  std::vector<float> result(size);

  for(std::size_t i = 0; i < size; ++i) {
    unsigned long seed = static_cast<unsigned long>(std::time(nullptr)) + rank;
    std::mt19937 gen(seed);  // Define range for float data type between 1 and 100
    std::uniform_real_distribution<float> dist(0.0f, 100.0f);
    result[i] = dist(gen);
  }

  return result;
}

int main(int argc, char *argv[]) {
  int rank, size;
  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  if(argc < 6) {
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
  int role = std::stoi(argv[5]);

  mpi_sleep(5, rank, "start");

  if(role == 0 || role == -1){
    MPI_Barrier(MPI_COMM_WORLD);
    adios2::ADIOS adios(config_path, MPI_COMM_WORLD);
    adios2::IO io = adios.DeclareIO("TestIO");

    std::vector<float> data(B);

    MPI_Barrier(MPI_COMM_WORLD);
    auto var_data = io.DefineVariable<float>("U", {size_t(size), B}, {size_t(rank), 0},
                                             {1, B}, adios2::ConstantDims);
    auto var_data2 = io.DefineVariable<float>("V", {size_t(size), B}, {size_t(rank), 0},
                                             {1, B}, adios2::ConstantDims);
    auto data_mag = io.DefineDerivedVariable("pdfU",
                                             "x:U \n"
                                             "magnitude(x)",
                                             adios2::DerivedVarType::StoreData);
    auto data_mag2 = io.DefineDerivedVariable("pdfV",
                                             "x:V \n"
                                             "magnitude(x)",
                                             adios2::DerivedVarType::StoreData);

    MPI_Barrier(MPI_COMM_WORLD);
    auto engine = io.Open(out_file, adios2::Mode::Write);
    for (int i = 0; i < N; ++i) {
      data = generateRandomVector(B);
      engine.BeginStep();
      mpi_sleep(5, rank, "beginstep");
      engine.Put<float>(var_data, data.data());
      engine.Put<float>(var_data2, data.data());
      mpi_sleep(5, rank, "put");
      engine.EndStep();
      mpi_sleep(5, rank, "endstep");
    }
    engine.Close();

    MPI_Barrier(MPI_COMM_WORLD);
  }

  MPI_Barrier(MPI_COMM_WORLD);

  if(role == 1 || role == -1){
    adios2::ADIOS adios(config_path, MPI_COMM_WORLD);
    adios2::IO io = adios.DeclareIO("TestIO");
    auto readEngine = io.Open(out_file, adios2::Mode::Read);

    std::vector<float> data;
    std::vector<float> derivedData;

    MPI_Barrier(MPI_COMM_WORLD);

    while (readEngine.BeginStep() == adios2::StepStatus::OK) {
      mpi_sleep(5, rank, "beginstep");
      adios2::Variable<float> readVariable = io.InquireVariable<float>("data");
      adios2::Variable<float> derVariable = io.InquireVariable<float>("data_mag");

      readEngine.Get<float>(readVariable, data);
      readEngine.Get<float>(derVariable, derivedData);
      mpi_sleep(3, rank, "read");
      readEngine.EndStep();
      mpi_sleep(5, rank, "endstep");
    }
    readEngine.Close();

    MPI_Barrier(MPI_COMM_WORLD);
  }

  MPI_Finalize();
  return 0;
}
