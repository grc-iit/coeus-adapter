//
// Created by Leo on 5/29/2025.
//

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
    adios2::ADIOS ad("adios2_config.xml", comm);
    adios2::IO reader_io = ad.DeclareIO("solution-io");
    adios2::IO writer_io = ad.DeclareIO("PDFAnalysisOutput");

    if (rank == 0) {
        std::cout << "Reading from: " << in_filename << " using engine: " << reader_io.EngineType() << std::endl;
        std::cout << "Writing to: " << out_filename << " using engine: " << writer_io.EngineType() << std::endl;
    }

    adios2::Engine reader = reader_io.Open(in_filename, adios2::Mode::Read, comm);
    std::vector<uint8_t> hashing_value_1;
    int stepAnalysis = 0;

    while (true) {
        adios2::StepStatus read_status = reader.BeginStep(adios2::StepMode::Read, 10.0f);
        if (read_status == adios2::StepStatus::NotReady) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            continue;
        } else if (read_status != adios2::StepStatus::OK) {
            break;
        }
        auto availableVars = reader_io.AvailableVariables();
        for (const auto &varEntry : availableVars) {
            const std::string &varName = varEntry.first;

            // Check if varName starts with "add"
            if (varName.rfind("add", 0) == 0) {
                const std::string &typeStr = varEntry.second.at("Type");
                std::cout << "Variable: " << varName << " | Type: " << typeStr << " | Values: ";

                if (typeStr == "int") {
                    auto var = reader_io.InquireVariable<int>(varName);
                    std::vector<int> data(var.Shape()[0]);
                    reader.Get(var, data);
                    for (int val : data) std::cout << val << ", ";
                }
                else if (typeStr == "uint8_t") {
                    auto var = reader_io.InquireVariable<uint8_t>(varName);
                    std::vector<uint8_t> data(var.Shape()[0]);
                    reader.Get(var, data);
                    for (uint8_t val : data) std::cout << static_cast<int>(val) << ", ";
                }
                else if (typeStr == "string" || typeStr == "std::string") {
                    auto var = reader_io.InquireVariable<std::string>(varName);
                    std::string value;
                    reader.Get(var, value);
                    std::cout << value;
                }
                else if (typeStr == "float") {
                    auto var = reader_io.InquireVariable<float>(varName);
                    std::vector<float> data(var.Shape()[0]);
                    reader.Get(var, data);
                    for (float val : data) std::cout << val << ", ";
                }
                else if (typeStr == "double") {
                    auto var = reader_io.InquireVariable<double>(varName);
                    std::vector<double> data(var.Shape()[0]);
                    reader.Get(var, data);
                    for (double val : data) std::cout << val << ", ";
                }
                else {
                    std::cout << "[Unsupported type: " << typeStr << "]";
                }

                std::cout << std::endl;
            }
        }

        reader.EndStep();
        ++stepAnalysis;
    }

    reader.Close();

    auto app_end_time = std::chrono::high_resolution_clock::now();
    auto app_duration = std::chrono::duration_cast<std::chrono::milliseconds>(app_end_time - app_start_time);

    std::cout << "Rank: " << rank << ", Duration: " << app_duration.count() << " ms" << std::endl;

    MPI_Barrier(comm);
    MPI_Finalize();
    return 0;
}
