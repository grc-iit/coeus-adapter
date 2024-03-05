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
#include <chrono>

#include "spdlog/logger.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/basic_file_sink.h"

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

bool epsilon(double d) { return (d < 1.0e-20); }
bool epsilon(float d) { return (d < 1.0e-20); }

/*
 * Function to compute the PDF of a 2D slice
 */
template <class T>
void compute_pdf(const std::vector<T> &data,
                 const std::vector<std::size_t> &shape, const size_t start,
                 const size_t count, const size_t nbins, const T min,
                 const T max, std::vector<T> &pdf, std::vector<T> &bins)
{
  if (shape.size() != 3)
    throw std::invalid_argument("ERROR: shape is expected to be 3D\n");

  size_t slice_size = shape[1] * shape[2];
  pdf.resize(count * nbins);
  bins.resize(nbins);

  size_t start_data = 0;
  size_t start_pdf = 0;

  T binWidth = (max - min) / nbins;
  for (auto i = 0; i < nbins; ++i)
  {
    bins[i] = min + (i * binWidth);
  }

  if (nbins == 1)
  {
    // special case: only one bin
    for (auto i = 0; i < count; ++i)
    {
      pdf[i] = slice_size;
    }
    return;
  }

  if (epsilon(max - min) || epsilon(binWidth))
  {
    // special case: constant array
    for (auto i = 0; i < count; ++i)
    {
      pdf[i * nbins + (nbins / 2)] = slice_size;
    }
    return;
  }

  for (auto i = 0; i < count; ++i)
  {
    // Calculate a PDF for 'nbins' bins for values between 'min' and 'max'
    // from data[ start_data .. start_data+slice_size-1 ]
    // into pdf[ start_pdf .. start_pdf+nbins-1 ]
    for (auto j = 0; j < slice_size; ++j)
    {
      if (data[start_data + j] > max || data[start_data + j] < min)
      {
        std::cout << " data[" << start * slice_size + start_data + j
                  << "] = " << data[start_data + j]
                  << " is out of [min,max] = [" << min << "," << max
                  << "]" << std::endl;
      }
      size_t bin = static_cast<size_t>(
          std::floor((data[start_data + j] - min) / binWidth));
      if (bin == nbins)
      {
        bin = nbins - 1;
      }
      ++pdf[start_pdf + bin];
    }
    start_pdf += nbins;
    start_data += slice_size;
  }
  return;
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
  size_t nbins = 100;
  bool write_inputvars = false;
  in_filename = argv[1];
  out_filename = argv[2];
  bool derived;

  if (argc >= 4)
  {
    int value = std::stoi(argv[3]);
    if (value > 0)
      nbins = static_cast<size_t>(value);
  }

  if (argc >= 5)
  {
    std::string value = argv[4];
    std::transform(value.begin(), value.end(), value.begin(), ::tolower);
    if (value == "yes") {
        write_inputvars = true;
    }
    derived = atoi(argv[5]);
  }

    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(spdlog::level::debug);
    console_sink->set_pattern("%v");
    spdlog::logger logger("debug_logger", { console_sink });
    logger.set_level(spdlog::level::debug);


    std::size_t u_global_size, v_global_size;
  std::size_t u_local_size, v_local_size;

  bool firstStep = true;

  std::vector<std::size_t> shape;
  size_t count1;
  size_t start1;

  std::vector<double> u;
  std::vector<double> v;
  int simStep = 0;

  std::vector<double> pdf_u;
  std::vector<double> pdf_v;
  std::vector<double> bins_u;
  std::vector<double> bins_v;

  // adios2 variable declarations
  adios2::Variable<double> var_u_in, var_v_in;
  adios2::Variable<int> var_step_in;
  adios2::Variable<double> var_u_pdf, var_v_pdf;
  adios2::Variable<double> var_u_bins, var_v_bins;
  adios2::Variable<int> var_step_out;
  adios2::Variable<double> var_u_out, var_v_out;
  std::cout << "flag3.1" << std::endl;
  // adios2 io object and engine init
  adios2::ADIOS ad("adios2.xml", comm);

  // IO objects for reading and writing
  adios2::IO reader_io = ad.DeclareIO("SimulationOutput");
  std::cout << "flag3.2" << std::endl;
  if (!rank)
  {
    std::cout << "PDF analysis reads from Simulation using engine type:  "
              << reader_io.EngineType() << std::endl;
  }

  // Engines for reading and writing
  adios2::Engine reader =
      reader_io.Open(in_filename, adios2::Mode::Read, comm);
    std::cout << "flag3.3" << std::endl;
  // read data step-by-step
    auto app_start_time = std::chrono::high_resolution_clock::now(); // Record end time of the application
    int stepAnalysis = 0;
  while (true)
  {
    // Begin read step
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

    // int stepSimOut = reader.CurrentStep();
    int stepSimOut = stepAnalysis;
      std::cout << "flag3.4" << std::endl;
    // Inquire variable
    var_u_in = reader_io.InquireVariable<double>("U");
    var_v_in = reader_io.InquireVariable<double>("V");
    var_u_pdf = reader_io.InquireVariable<double>("derive/pdfU");
    var_v_pdf = reader_io.InquireVariable<double>("derive/pdfV");
    var_step_in = reader_io.InquireVariable<int>("step");

    // Set the selection at the first step only, assuming that
    // the variable dimensions do not change across timesteps
    if (firstStep){
      shape = var_u_in.Shape();
        std::cout << "flag3.5" << std::endl;
      // Calculate global and local sizes of U and V
      u_global_size = shape[0] * shape[1] * shape[2];
      u_local_size = u_global_size / comm_size;
      v_global_size = shape[0] * shape[1] * shape[2];
      v_local_size = v_global_size / comm_size;
        std::cout << "flag3.6" << std::endl;
      // 1D decomposition
      count1 = shape[0] / comm_size;
      start1 = count1 * rank;
      if (rank == comm_size - 1){
        // last process need to read all the rest of slices
        count1 = shape[0] - count1 * (comm_size - 1);
      }

      // Set selection
      var_u_in.SetSelection(adios2::Box<adios2::Dims>(
          {start1, 0, 0}, {count1, shape[1], shape[2]}));
      var_v_in.SetSelection(adios2::Box<adios2::Dims>(
          {start1, 0, 0}, {count1, shape[1], shape[2]}));

      firstStep = false;
    }

    if(derived == 0) {
        reader.Get<double>(var_u_in, u);
        reader.Get<double>(var_v_in, v);
    }
    if(derived == 1) {
      reader.Get<double>(var_u_pdf, pdf_u);
      reader.Get<double>(var_v_pdf, pdf_v);
    }

    // End read step (let resources about step go)
    reader.EndStep();

    if (!rank){
      std::cout << "PDF Analysis step " << stepAnalysis
                << " processing sim output step " << stepSimOut << std::endl;
    }

    std::vector<double> pdf_u;
    std::vector<double> bins_u;
    std::vector<double> pdf_v;
    std::vector<double> bins_v;

    if(derived == 0) {
        // Calculate min/max of arrays
        std::pair<double, double> minmax_u;
        std::pair<double, double> minmax_v;
        auto mmu = std::minmax_element(u.begin(), u.end());
        minmax_u = std::make_pair(*mmu.first, *mmu.second);
        auto mmv = std::minmax_element(v.begin(), v.end());
        minmax_v = std::make_pair(*mmv.first, *mmv.second);

        // Compute PDF
        compute_pdf(u, shape, start1, count1, nbins, minmax_u.first,
                    minmax_u.second, pdf_u, bins_u);
        compute_pdf(v, shape, start1, count1, nbins, minmax_v.first,
                    minmax_v.second, pdf_v, bins_v);
    }

    ++stepAnalysis;
  }

  // cleanup (close reader and writer)
  reader.Close();

  MPI_Barrier(comm);
    auto app_end_time = std::chrono::high_resolution_clock::now(); // Record end time of the application
    auto app_duration = std::chrono::duration_cast<std::chrono::milliseconds>(app_end_time - app_start_time);
    logger.info("Rank {} - ET {} - milliseconds", rank, app_duration.count());

  MPI_Finalize();
  return 0;
}