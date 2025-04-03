
#include <adios2.h>
#include <mpi.h>
#include <iostream>
#include <vector>
#include <map>
#include <chrono>
#include <thread>
#include <numeric>

// Helper function to compute the total size of a variable
size_t GetTotalSize(const std::vector<std::size_t> &shape) {
    return shape.empty() ? 1 : std::accumulate(shape.begin(), shape.end(), 1, std::multiplies<std::size_t>());
}

int main(int argc, char **argv) {
    auto app_start_time = std::chrono::high_resolution_clock::now();
    MPI_Init(&argc, &argv);

    int rank, comm_size, wrank;
    MPI_Comm_rank(MPI_COMM_WORLD, &wrank);

    const unsigned int color = 2;
    MPI_Comm comm;
    MPI_Comm_split(MPI_COMM_WORLD, color, wrank, &comm);
    MPI_Comm_rank(comm, &rank);
    MPI_Comm_size(comm, &comm_size);

    if (argc < 3) {
        if (rank == 0) {
            std::cerr << "Usage: " << argv[0] << " <input_bp5> <output_bp5>" << std::endl;
        }
        MPI_Finalize();
        return EXIT_FAILURE;
    }

    std::string in_filename = argv[1];
    std::string out_filename = argv[2];

    bool firstStep = true;

    // Initialize ADIOS2
    adios2::ADIOS ad("adios2.xml", comm);
    adios2::IO reader_io = ad.DeclareIO("wrfout_d01_2019-11-26_12:00:00");
    adios2::IO writer_io = ad.DeclareIO("PDFAnalysisOutput");

    if (rank == 0) {
        std::cout << "Reading from: " << in_filename << " using engine: " << reader_io.EngineType() << std::endl;
        std::cout << "Writing to: " << out_filename << " using engine: " << writer_io.EngineType() << std::endl;
    }

    adios2::Engine reader = reader_io.Open(in_filename, adios2::Mode::Read, comm);
    adios2::Engine writer = writer_io.Open(out_filename, adios2::Mode::Read, comm);
    std::vector<uint8_t> hashing_value_1;
    std::vector<uint8_t> hashing_value_2;
    int stepAnalysis = 0;

    while (true) {
        adios2::StepStatus read_status = reader.BeginStep(adios2::StepMode::Read, 10.0f);
        if (read_status == adios2::StepStatus::NotReady) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            continue;
        } else if (read_status != adios2::StepStatus::OK) {
            break;
        }
        writer.BeginStep();
        auto availableVars = writer_io.AvailableVariables();

            for (const auto &varEntry : availableVars) {
                const std::string &varName = varEntry.first;

                if (varEntry.second.at("Type") == "uint8_t") {
                    std::cout << varName << std::endl;
                    auto var = reader_io.InquireVariable<uint8_t>(varName);
                    auto var1 = writer_io.InquireVariable<uint8_t>(varName);
                    reader.Get(var, hashing_value_1);
                    writer.Get(var1, hashing_value_2);
                    for(int i =0; i < hashing_value_1.size(); i++){
                        std::cout << static_cast<int>(hashing_value_1[i]) << " value: " << static_cast<int>(hashing_value_2[i]) << std::endl;
                        if (static_cast<int>(hashing_value_1[i]) - static_cast<int>(hashing_value_2[i]) > 0.01) {
                            auto app_end_time = std::chrono::system_clock::now();
                            std::time_t end_time_t = std::chrono::system_clock::to_time_t(app_end_time);
                            std::cout << "The difference happened at: " << std::ctime(&end_time_t) << std::endl;
                        }
                    }
                }
            }


        writer.EndStep();  // End step for all variables
        reader.EndStep();
        ++stepAnalysis;
    }

    reader.Close();
    writer.Close();

    auto app_end_time = std::chrono::high_resolution_clock::now();
    auto app_duration = std::chrono::duration_cast<std::chrono::milliseconds>(app_end_time - app_start_time);

    std::cout << "Rank: " << rank << ", Duration: " << app_duration.count() << " ms" << std::endl;

    MPI_Barrier(comm);
    MPI_Finalize();
    return 0;
}
