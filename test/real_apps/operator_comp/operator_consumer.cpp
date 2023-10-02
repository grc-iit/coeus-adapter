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

  if (argc < 4) {
    if (rank == 0) {
      std::cerr << "Please provide the engine name. Adios or hermes." << std::endl;
    }
    MPI_Finalize();
    return -1;
  }

  std::string engine_name = argv[1];  // Number of steps
  std::string config_file = argv[2];
  std::string in_file = argv[3];

  adios2::ADIOS adios(config_file, MPI_COMM_WORLD);
  adios2::IO io = adios.DeclareIO("TestIO");
  adios2::Engine engine = io.Open(in_file, adios2::Mode::Read);

  adios2::Variable<double> var;
  adios2::Variable<double> normVec;
  adios2::Variable<double> diffVec;

  double normValue;
  std::vector<double> diffValue;

  std::vector<double> current_data(3), previous_data(3, 0);

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

    if(engine_name == "adios") {
      auto start = std::chrono::high_resolution_clock::now();
      engine.Get(var, current_data);

      normValue = norm(current_data);

      diffValue[0] = current_data[0] - previous_data[0];
      diffValue[1] = current_data[1] - previous_data[1];
      diffValue[2] = current_data[2] - previous_data[2];

      auto stop = std::chrono::high_resolution_clock::now();
      previous_data = current_data;
      accumulated_time += std::chrono::duration<double, std::micro>(stop - start).count();
    }
    else if(engine_name == "hermes"){
      auto start = std::chrono::high_resolution_clock::now();

      engine.Get(normVec, normValue);
      engine.Get(diffVec, diffValue);

      auto stop = std::chrono::high_resolution_clock::now();
      accumulated_time += std::chrono::duration<double, std::micro>(stop - start).count();
    }
    else{
      if (rank == 0) {
        std::cerr << "wrong engine name. Use adios or hermes." << std::endl;
      }
      MPI_Finalize();
      return -1;
    }
    engine.EndStep();

    step++;
  }

  engine.Close();
  MPI_Barrier(MPI_COMM_WORLD);
  if(rank==0){
    std::cout << "Total Get time: " << accumulated_time << std::endl;
  }

  double total_time;
  MPI_Reduce(&accumulated_time, &total_time, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
  MPI_Barrier(MPI_COMM_WORLD);

  if(rank == 0) {
    auto filename = "operator_consumer_results.csv";
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
    outputFile << size << "," << step << "," << total_time << std::endl;
    outputFile.close();
  }

  MPI_Finalize();
  return 0;
}