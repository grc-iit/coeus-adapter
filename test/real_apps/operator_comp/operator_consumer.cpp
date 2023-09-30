  #include <adios2.h>
#include <vector>
#include <cmath>
#include <chrono>
#include <iostream>
#include <fstream>

double norm(const std::vector<double>& vec) {
  return std::sqrt(vec[0]*vec[0] + vec[1]*vec[1] + vec[2]*vec[2]);
}

int main(int argc, char* argv[]) {
  MPI_Init(&argc, &argv);

  int rank, size;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  if (argc < 2) {
    if (rank == 0) {
      std::cerr << "Please provide the engine name. Adios or hermes." << std::endl;
    }
    MPI_Finalize();
    return -1;
  }

  std::string engine_name = argv[1];  // Number of steps

  adios2::ADIOS adios("operator_comp.xml", MPI_COMM_WORLD);
  adios2::IO io = adios.DeclareIO("TestIO");
  adios2::Engine engine = io.Open("data.bp", adios2::Mode::Read);

  adios2::Variable<double> var;
  adios2::Variable<double> normVec;
  adios2::Variable<double> diffVec;

  double normValue;
  double diffValue;

  std::vector<double> current_data(3), previous_data(3);

  double accumulated_time = 0.0;
  int step = 0;

  while (engine.BeginStep() == adios2::StepStatus::OK) {
    if(engine_name == "adios") {
      var = io.InquireVariable<double>("vector");
    }
    else if(engine_name == "hermes"){
      normVec = io.InquireVariable<double>("norm");
      diffVec = io.InquireVariable<double>("diff");
    }
    else{
      if (rank == 0) {
        std::cerr << "wrong engine name. Use adios or hermes." << std::endl;
      }
      MPI_Finalize();
      return -1;
    }

    auto start = std::chrono::high_resolution_clock::now();

    if(engine_name == "adios") {
      engine.Get(var, current_data.data());
      engine.EndStep();

      double current_norm = norm(current_data);

      if (step > 0) {
        double diff_x = current_data[0] - previous_data[0];
        double diff_y = current_data[1] - previous_data[1];
        double diff_z = current_data[2] - previous_data[2];
      }
    }
    else if(engine_name == "hermes"){
      engine.Get(normVec, normValue);
      engine.Get(diffVec, diffValue);
      engine.EndStep();
    }
    else{
      if (rank == 0) {
        std::cerr << "wrong engine name. Use adios or hermes." << std::endl;
      }
      MPI_Finalize();
      return -1;
    }

    auto stop = std::chrono::high_resolution_clock::now();
    accumulated_time += std::chrono::duration<double, std::micro>(stop - start).count();

    previous_data = current_data;
    step++;
  }

  engine.Close();

  double total_time;
  MPI_Reduce(&accumulated_time, &total_time, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);

  MPI_Barrier(MPI_COMM_WORLD);

  if (rank == 0) {
    std::ofstream outputFile("operator_consumer_results.csv", std::ios_base::app); // Append to the file
    outputFile << size << "," << step << "," << total_time << std::endl;
    outputFile.close();
  }

  MPI_Finalize();
  return 0;
}