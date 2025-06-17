//
// Created by Leo on 10/4/2024.
//
/*
 * Analysis code for the Gray-Scott application.
 * Reads variable U and V, and computes the PDF for each 2D slices of U and V.
 * Writes the computed PDFs using ADIOS.
 *
 * Norbert Podhorszki, pnorbert@ornl.gov
 *
 */

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "adios2.h"
#include <mpi.h>

std::string concatenateVectorToString(const std::vector<size_t> &vec) {
    std::stringstream ss;
    ss << "( ";
    for (size_t i = 0; i < vec.size(); ++i) {
        ss << vec[i];
        if (i < vec.size() - 1) {
            ss << ", ";
        }
    }
    ss << " )";
    return ss.str();
}


/*
 * Print info to the user on how to invoke the application
 */
void printUsage()
{
    std::cout
            << "Usage: pdf_calc input output [N] [output_inputdata]\n"
            << "  input:   Name of the input file handle for reading data\n"
            << "  output:  Name of the output file to which data must be written\n"
            << "  N:       Number of bins for the PDF calculation, default = 1000\n"
            << "  output_inputdata: YES will write the original variables besides "
               "the analysis results\n\n";
}

/*
 * MAIN
 */
int main(int argc, char *argv[])
{
    auto app_start_time = std::chrono::high_resolution_clock::now();
    MPI_Init(&argc, &argv);
    int rank, comm_size, wrank;

    MPI_Comm_rank(MPI_COMM_WORLD, &wrank);

    const unsigned int color = 2;
    MPI_Comm comm;
    MPI_Comm_split(MPI_COMM_WORLD, color, wrank, &comm);

    MPI_Comm_rank(comm, &rank);
    MPI_Comm_size(comm, &comm_size);

    if (argc < 3)
    {
        std::cout << "Not enough arguments\n";
        if (rank == 0)
            printUsage();
        MPI_Finalize();
        return 0;
    }

    std::string in_filename;
    std::string out_filename;

    bool write_inputvars = false;
    in_filename = argv[1];
    out_filename = argv[2];


    std::size_t u_global_size, v_global_size;
    std::size_t u_local_size, v_local_size;
    bool firstStep = true;
    std::vector<std::size_t> shape;
    size_t count1;
    size_t start1;
    int simStep = -5;

    // adios2 variable declarations
    adios2::Variable<double> var_u_in, var_v_in;
    adios2::Variable<int> var_step_in;
    adios2::Variable<int> var_step_out;
    // another variable from copy
    adios2::Variable<int> var_step_in_2;
    adios2::Variable<int> var_step_out_2;
    std::vector<uint8_t> readHashV_1;
    std::vector<uint8_t> readHashU_1;
    std::vector<uint8_t> readHashV_2;
    std::vector<uint8_t> readHashU_2;
    // adios2 io object and engine init
    adios2::ADIOS ad("adios2.xml", comm);

    // IO objects for reading and writing
    adios2::IO reader_io = ad.DeclareIO("SimulationOutput");
    adios2::IO reader2_io = ad.DeclareIO("PDFAnalysisOutput");
    if (!rank)
    {
        std::cout << "PDF analysis reads from Simulation using engine type:  "
                  << reader_io.EngineType() << std::endl;
        std::cout << "PDF analysis writes using engine type:                 "
                  << reader2_io.EngineType() << std::endl;
    }

    // Engines for reading and writing
    adios2::Engine reader =
            reader_io.Open(in_filename, adios2::Mode::Read, comm);
    adios2::Engine reader_2 =
            reader2_io.Open(out_filename, adios2::Mode::Read, comm);

    bool shouldIWrite = (!rank || reader_io.EngineType() == "HDF5");

    // read data step-by-step
    int stepAnalysis = 0;

    while (true)
    {
        // Begin read step
        adios2::StepStatus read_status =
                reader.BeginStep(adios2::StepMode::Read, 10.0f);
        if (read_status == adios2::StepStatus::NotReady)
        {

            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            continue;
        }
        else if (read_status != adios2::StepStatus::OK)
        {
            break;
        }

        // int stepSimOut = reader.CurrentStep();
        int stepSimOut = stepAnalysis;

        adios2::StepStatus read_status_2 =
                reader_2.BeginStep(adios2::StepMode::Read, 10.0f);
        if (read_status_2 == adios2::StepStatus::NotReady)
        {
            // std::cout << "Stream not ready yet. Waiting...\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            continue;
        }
        else if (read_status_2 != adios2::StepStatus::OK)
        {
            break;
        }

        // int stepSimOut = reader.CurrentStep();
        int stepSimOut_2 = stepAnalysis;
        var_u_in = reader_io.InquireVariable<double>("U");


        // Set the selection at the first step only, assuming that
        // the variable dimensions do not change across timesteps
        if (firstStep) {
            shape = var_u_in.Shape();
            // Calculate global and local sizes of U and V
            u_global_size = shape[0] * shape[1] * shape[2];
            u_local_size = u_global_size / comm_size;
            v_global_size = shape[0] * shape[1] * shape[2];
            v_local_size = v_global_size / comm_size;
            // 1D decomposition
            count1 = shape[0] / comm_size;
            start1 = count1 * rank;
            if (rank == comm_size - 1) {
                // last process need to read all the rest of slices
                count1 = shape[0] - count1 * (comm_size - 1);
            }
            if (write_inputvars) {
            }
            firstStep = false;
        }

//        var_u_in.SetSelection(adios2::Box<adios2::Dims>(
//                {start1, 0, 0}, {count1, shape[1], shape[2]}));
//        var_v_in.SetSelection(adios2::Box<adios2::Dims>(
//                {start1, 0, 0}, {count1, shape[1], shape[2]}));

        auto varhash_U_1 = reader_io.InquireVariable<uint8_t>("derive/hashU");
        auto varhash_V_1 = reader_io.InquireVariable<uint8_t>("derive/hashV");
        auto varhash_U_2 = reader2_io.InquireVariable<uint8_t>("derive/hashU");
        auto varhash_V_2 = reader2_io.InquireVariable<uint8_t>("derive/hashV");
        reader.Get(varhash_U_1, readHashU_1);
        reader.Get(varhash_V_1, readHashV_1);
        reader_2.Get(varhash_U_2, readHashV_2);
        reader_2.Get(varhash_V_2, readHashU_2);
        // compare the hash value
        for(int i =0; i < readHashV_1.size(); i++){
            std::cout << static_cast<int>(readHashV_1[i]) << " value: " << static_cast<int>(readHashV_2[i]) << std::endl;
            if (static_cast<int>(readHashV_1[i]) - static_cast<int>(readHashV_2[i]) > 0.01) {
                auto app_end_time = std::chrono::system_clock::now();
                std::time_t end_time_t = std::chrono::system_clock::to_time_t(app_end_time);
                std::cout << "The difference happened at: " << std::ctime(&end_time_t) << std::endl;
            }
            if (static_cast<int>(readHashU_1[i]) - static_cast<int>(readHashU_2[i]) > 0.01) {
                auto app_end_time = std::chrono::system_clock::now();
                std::time_t end_time_t = std::chrono::system_clock::to_time_t(app_end_time);
                std::cout <<"The difference happened at: " << std::ctime(&end_time_t) << std::endl;
            }

        }

        // End read step (let resources about step go)
        reader.EndStep();
        reader_2.EndStep();
        ++stepAnalysis;
    }


    reader.Close();
    reader_2.Close();
    auto app_end_time = std::chrono::high_resolution_clock::now(); // Record end time of the application
    auto app_duration = std::chrono::duration_cast<std::chrono::milliseconds>(app_end_time - app_start_time);
    std::cout << "rank:" << rank << ", time: " <<  app_duration.count() << std::endl;
    MPI_Barrier(comm);
    MPI_Finalize();
    return 0;
}