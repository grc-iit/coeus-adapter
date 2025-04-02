#include <adios2.h>
#include <mpi.h>
#include <iostream>
#include <vector>
#include <map>
#include <chrono>
#include <thread>

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
    std::cout << in_filename << std::endl;
    std::string out_filename = argv[2];
    std::cout << out_filename << std::endl;
    bool firstStep = true;
    int simStep = -5;

    // Initialize ADIOS2
    adios2::ADIOS ad("adios2.xml", comm);
    adios2::IO reader_io = ad.DeclareIO("wrfout_d01_2019-11-26_12:00:00");
    adios2::IO writer_io = ad.DeclareIO("PDFAnalysisOutput");

    if (rank == 0) {
        std::cout << "Reading from: " << in_filename << " using engine: " << reader_io.EngineType() << std::endl;
        std::cout << "Writing to: " << out_filename << " using engine: " << writer_io.EngineType() << std::endl;
    }

    adios2::Engine reader = reader_io.Open(in_filename, adios2::Mode::Read, comm);
    adios2::Engine writer = writer_io.Open(out_filename, adios2::Mode::Write, comm);

    int stepAnalysis = 0;
    std::cout << "flag1" << std::endl;
    while (true) {
        adios2::StepStatus read_status = reader.BeginStep(adios2::StepMode::Read, 10.0f);
        if (read_status == adios2::StepStatus::NotReady) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            continue;
        } else if (read_status != adios2::StepStatus::OK) {
            std::cout << "flag 1.1" << std::endl;
            break;
        }

        int stepSimOut = stepAnalysis;
        auto availableVars = reader_io.AvailableVariables();

        if (firstStep) {
            std::cout << "flag2" << std::endl;
            for (const auto &varEntry : availableVars) {
                const std::string &varName = varEntry.first;
                auto var = reader_io.InquireVariable<double>(varName);
                if (var) {
                    writer_io.DefineVariable<double>(varName, var.Shape(), var.Start(), var.Count(), adios2::ConstantDims);
                }
            }
            firstStep = false;
        }
        std::cout << "flag3" << std::endl;
        // Read and write all variables
        for (const auto &varEntry : availableVars) {
            const std::string &varName = varEntry.first;
            auto var = reader_io.InquireVariable<double>(varName);
            if (var) {
                std::cout << varName << std::endl;
                std::vector<double> data(var.Shape()[0]); // Assume 1D for simplicity
                reader.Get(var, data, adios2::Mode::Sync);
                writer.BeginStep();
                writer.Put(writer_io.InquireVariable<double>(varName), data.data());
                writer.EndStep();
            }
        }

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
