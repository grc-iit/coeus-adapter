#include <adios2.h>
#include <iostream>
#include <vector>
#include <mpi.h>
int main(int argc, char *argv[]) {
    // Initialize MPI (if needed)
    int rank, size;
    //#if ADIOS2_HAVE_MPI

    MPI_Init(&argc, &argv);

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);


    adios2::ADIOS adios(MPI_COMM_WORLD);
    adios2::IO reader_io = adios.DeclareIO("BPFileReader");

    // Open the BP5 file for reading
    adios2::Engine bpFileReader = reader_io.Open("out1.bp", adios2::Mode::Read);

    // Inquire the variables U, V, and step


    if (!var_u_in || !var_v_in || !var_step_in) {
        std::cerr << "Error: One or more variables not found in the input file!" << std::endl;
        MPI_Finalize();
        return -1;
    }

    // Prepare for reading data
    std::vector<double> u_data, v_data;
    std::vector<int> step_data;

    // Define the output file
    adios2::IO writer_io = adios.DeclareIO("BPFileWriter");
    adios2::Engine bpFileWriter = writer_io.Open("output2.bp", adios2::Mode::Write);

    while (true) {
        // Allocate memory for variables based on the current step's variable size
        adios2::StepStatus read_status =
                reader.BeginStep(adios2::StepMode::Read, 10.0f);
        if (read_status == adios2::StepStatus::NotReady)
        {
            // std::cout << "Stream not ready yet. Waiting...\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            continue;
        }
        else if (read_status != adios2::StepStatus::OK)
        {
            break;
        }
        auto var_u_in = reader_io.InquireVariable<double>("U");
        auto var_v_in = reader_io.InquireVariable<double>("V");
        auto var_step_in = reader_io.InquireVariable<int>("step");

        auto u_shape = var_u_in.Shape();
        auto v_shape = var_v_in.Shape();
        size_t u_size = u_shape[0];  // Assuming 1D data
        size_t v_size = v_shape[0];

        u_data.resize(u_size);
        v_data.resize(v_size);
        step_data.resize(1);  // Assuming scalar for 'step'

        // Get data for U, V, and step
        bpFileReader.Get(var_u_in, u_data.data());
        bpFileReader.Get(var_v_in, v_data.data());
        bpFileReader.Get(var_step_in, step_data.data());

        // End the reading step
        bpFileReader.EndStep();
        // Define variables for writing (U1, V1, step1)
        auto var_u_out = writer_io.DefineVariable<double>("U1", {u_size}, {0}, {u_size});
        auto var_v_out = writer_io.DefineVariable<double>("V1", {v_size}, {0}, {v_size});
        auto var_step_out = writer_io.DefineVariable<int>("step1", {1}, {0}, {1});

        // Write to the new BP file
        bpFileWriter.BeginStep();
        bpFileWriter.Put(var_u_out, u_data.data());
        bpFileWriter.Put(var_v_out, v_data.data());
        bpFileWriter.Put(var_step_out, step_data.data());
        bpFileWriter.EndStep();
    }

    // Close the reader and writer engines
    bpFileReader.Close();
    bpFileWriter.Close();

    // Finalize MPI (if needed)
    MPI_Finalize();

    return 0;
}
