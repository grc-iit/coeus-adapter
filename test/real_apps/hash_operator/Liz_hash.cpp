#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <string>
#include <sys/mman.h>
#include <unistd.h>
//#if ADIOS2_HAVE_MPI
#include <mpi.h>
//#endif
#include <cmath>
#include <iostream>
#include <numeric>
#include <random>
#include <stdexcept>
#include <vector>

#include <adios2.h>
#include <gtest/gtest.h>

template <typename T>
size_t
read_file(const std::string &fn, std::vector<T> &buff) {
    int fd = open(fn.c_str(), O_RDONLY);
    if (fd == -1) {
        std::string errMsg = "Cannot open " + fn;
        throw std::runtime_error(errMsg);
    }
    size_t size = lseek(fd, 0, SEEK_END);
    if (size == static_cast<size_t>(-1)) {
        close(fd);
        throw std::runtime_error("lseek failed");
    }
    assert(size % sizeof(T) == 0);
    buff.resize(size / sizeof(T));
    lseek(fd, 0, SEEK_SET);
    size_t transferred = 0, remaining = size;
    while (remaining > 0) {
        ssize_t ret = read(
                fd, reinterpret_cast<char *>(buff.data()) + transferred, remaining);
        if (ret < 0) {
            close(fd);
            std::string errMsg =
                    "Cannot read " + std::to_string(size) + " bytes from " + fn;
            throw std::runtime_error(errMsg);
        }
        remaining -= ret;
        transferred += ret;
    }
    close(fd);
    return size;
}

// Experimental
TEST(DerivedCorrectness, HashTest)
{
int rank, size;
//#if ADIOS2_HAVE_MPI

// MPI_THREAD_MULTIPLE is only required if you enable the SST MPI_DP
MPI_Init_thread(nullptr, nullptr, MPI_THREAD_MULTIPLE, &size);
MPI_Comm_rank(MPI_COMM_WORLD, &rank);
MPI_Comm_size(MPI_COMM_WORLD, &size);

//#endif
const size_t steps = 1;

std::string in_file = "/lustre/orion/csc143/proj-shared/dulac/sample/m000p.mpirestart-combined-0-10.dat";
std::string out_file = in_file + ".tree";

// Read Tree data
std::vector<float> in_buff;
size_t size_in = read_file(in_file, in_buff);
std::vector<uint8_t> out_buff;
size_t size_out = read_file(out_file, out_buff);
if (rank == 0) {
std::cout << "Hash data size in: " << size_in << std::endl;
std::cout << "size of in vector " << in_buff.size() << std::endl;
std::cout << "Hash tree size out: " << size_out << std::endl;
std::cout << "size of out vector " << out_buff.size() << std::endl;
}

//#if ADIOS2_HAVE_MPI
adios2::ADIOS adios(MPI_COMM_WORLD);
//std::cout << "Running example with mpi" << std::endl;
/*#else
adios2::ADIOS adios;
std::cout << "Running example withOUT mpi" << std::endl;
#endif*/

adios2::IO bpOut = adios.DeclareIO("BPWriteAddExpression");

std::string varname = "sim/data";
std::string derivedname = "derived/hash";

size_t in_count = (size_t)(in_buff.size() / size);
size_t in_shape = in_buff.size();
size_t in_start = in_count * rank;
//double count_ratio = 33 / 4096; // assume chunk size 4KB

std::cout << "Rank " << rank << " defining variables with shape ";
std::cout << in_shape << ", start " << in_start;
std::cout << ", count " << in_count << std::endl;
auto InputData = bpOut.DefineVariable<float>(varname, {in_shape}, {in_start}, {in_count});
// clang-format off
bpOut.DefineDerivedVariable(derivedname,
"x =" + varname + " \n"
"hash(x)",
adios2::DerivedVarType::StoreData);
// clang-format on
std::string filename = "expHash.bp";
adios2::Engine bpFileWriter = bpOut.Open(filename, adios2::Mode::Write);

for (size_t i = 0; i < steps; i++)
{
bpFileWriter.BeginStep();
bpFileWriter.Put(InputData, in_buff.data());
bpFileWriter.EndStep();
}
std::cout << "Rank " << rank << " calling writer Close" << std::endl;
bpFileWriter.Close();

adios2::IO bpIn = adios.DeclareIO("BPReadExpression");
adios2::Engine bpFileReader = bpIn.Open(filename, adios2::Mode::Read);

std::vector<float> readData;
std::vector<uint8_t> readHash;

float epsilon = (float)0.01;
for (size_t i = 0; i < steps; i++)
{
bpFileReader.BeginStep();
bpFileReader.Get(varname, readData);
std::cout << "Rank " << rank << " Inquiring Hash" << std::endl;
auto varhash = bpIn.InquireVariable<uint8_t>(derivedname);
std::cout << "Rank " << rank << " Getting Hash" << std::endl;
bpFileReader.Get(varhash, readHash);
std::cout << "Rank " << rank << " EndStep" << std::endl;
bpFileReader.EndStep();

std::cout << "Rank " << rank << " looping over out_buff.size() of " << out_buff.size() << std::endl;
for (size_t ind = 0; ind < out_buff.size(); ++ind)
{
//std::cout << "Hash[" << ind << "] = " << out_buff[ind] << " (" << readHash[ind] << " read) ";
//EXPECT_TRUE(fabs(readHash[ind] - out_buff[ind]) < epsilon);
}
}
std::cout << "Rank " << rank << " reader.Close()" << std::endl;
bpFileReader.Close();
//#if ADIOS2_HAVE_MPI
MPI_Finalize();
//#endif
}
/*
TEST(DerivedCorrectness, AddCorrectnessTest)
{
    const size_t Nx = 10, Ny = 3, Nz = 6;
    const size_t steps = 2;
    /** Application variable
    std::default_random_engine generator;
    std::uniform_real_distribution<float> distribution(0.0, 10.0);

    std::vector<float> simArray1(Nx * Ny * Nz);
    std::vector<float> simArray2(Nx * Ny * Nz);
    std::vector<float> simArray3(Nx * Ny * Nz);
    for (size_t i = 0; i < Nx * Ny * Nz; ++i)
    {
        simArray1[i] = distribution(generator);
        simArray2[i] = distribution(generator);
        simArray3[i] = distribution(generator);
    }

    adios2::ADIOS adios;

    adios2::IO bpOut = adios.DeclareIO("BPWriteAddExpression");

    std::vector<std::string> varname = {"sim1/Ux", "sim1/Uy", "sim1/Uz"};
    std::string derivedname = "derived/addU";

    auto Ux = bpOut.DefineVariable<float>(varname[0], {Nx, Ny, Nz}, {0, 0, 0}, {Nx, Ny, Nz});
    auto Uy = bpOut.DefineVariable<float>(varname[1], {Nx, Ny, Nz}, {0, 0, 0}, {Nx, Ny, Nz});
    auto Uz = bpOut.DefineVariable<float>(varname[2], {Nx, Ny, Nz}, {0, 0, 0}, {Nx, Ny, Nz});
    // clang-format off
    bpOut.DefineDerivedVariable(derivedname,
                                "x =" + varname[0] + " \n"
                                "y =" + varname[1] + " \n"
                                "z =" + varname[2] + " \n"
                                "x+y+z",
                                adios2::DerivedVarType::StoreData);
    // clang-format on
    std::string filename = "expAdd.bp";
    adios2::Engine bpFileWriter = bpOut.Open(filename, adios2::Mode::Write);

    for (size_t i = 0; i < steps; i++)
    {
        bpFileWriter.BeginStep();
        bpFileWriter.Put(Ux, simArray1.data());
        bpFileWriter.Put(Uy, simArray2.data());
        bpFileWriter.Put(Uz, simArray3.data());
        bpFileWriter.EndStep();
    }
    bpFileWriter.Close();

    adios2::IO bpIn = adios.DeclareIO("BPReadExpression");
    adios2::Engine bpFileReader = bpIn.Open(filename, adios2::Mode::Read);

    std::vector<float> readUx;
    std::vector<float> readUy;
    std::vector<float> readUz;
    //std::vector<float> readAdd;
    std::vector<uint8_t> readAdd;

    float calcA;
    float epsilon = (float)0.01;
    for (size_t i = 0; i < steps; i++)
    {
        bpFileReader.BeginStep();
        bpFileReader.Get(varname[0], readUx);
        bpFileReader.Get(varname[1], readUy);
        bpFileReader.Get(varname[2], readUz);
        bpFileReader.Get(derivedname, readAdd);
        bpFileReader.EndStep();

        for (size_t ind = 0; ind < Nx * Ny * Nz; ++ind)
        {
            calcA = readUx[ind] + readUy[ind] + readUz[ind];
            EXPECT_TRUE(fabs(calcA - (float)readAdd[ind]) < epsilon);
        }
    }
    bpFileReader.Close();
}

TEST(DerivedCorrectness, MagCorrectnessTest)
{
    const size_t Nx = 2, Ny = 3, Nz = 10;
    const size_t steps = 2;
    // Application variable
    std::default_random_engine generator;
    std::uniform_real_distribution<float> distribution(0.0, 10.0);

    std::vector<float> simArray1(Nx * Ny * Nz);
    std::vector<float> simArray2(Nx * Ny * Nz);
    std::vector<float> simArray3(Nx * Ny * Nz);
    for (size_t i = 0; i < Nx * Ny * Nz; ++i)
    {
        simArray1[i] = distribution(generator);
        simArray2[i] = distribution(generator);
        simArray3[i] = distribution(generator);
    }

    adios2::ADIOS adios;
    adios2::IO bpOut = adios.DeclareIO("BPWriteExpression");
    std::vector<std::string> varname = {"sim2/Ux", "sim2/Uy", "sim2/Uz"};
    std::string derivedname = "derived/magU";

    auto Ux = bpOut.DefineVariable<float>(varname[0], {Nx, Ny, Nz}, {0, 0, 0}, {Nx, Ny, Nz});
    auto Uy = bpOut.DefineVariable<float>(varname[1], {Nx, Ny, Nz}, {0, 0, 0}, {Nx, Ny, Nz});
    auto Uz = bpOut.DefineVariable<float>(varname[2], {Nx, Ny, Nz}, {0, 0, 0}, {Nx, Ny, Nz});
    // clang-format off
    bpOut.DefineDerivedVariable(derivedname,
                                "x =" + varname[0] + " \n"
                                "y =" + varname[1] + " \n"
                                "z =" + varname[2] + " \n"
                                "magnitude(x,y,z)",
                                adios2::DerivedVarType::StoreData);
    // clang-format on
    std::string filename = "expMagnitude.bp";
    adios2::Engine bpFileWriter = bpOut.Open(filename, adios2::Mode::Write);

    for (size_t i = 0; i < steps; i++)
    {
        bpFileWriter.BeginStep();
        bpFileWriter.Put(Ux, simArray1.data());
        bpFileWriter.Put(Uy, simArray2.data());
        bpFileWriter.Put(Uz, simArray3.data());
        bpFileWriter.EndStep();
    }
    bpFileWriter.Close();

    adios2::IO bpIn = adios.DeclareIO("BPReadMagExpression");
    adios2::Engine bpFileReader = bpIn.Open(filename, adios2::Mode::Read);

    std::vector<float> readUx;
    std::vector<float> readUy;
    std::vector<float> readUz;
    //std::vector<float> readMag;
    std::vector<uint8_t> readMag;

    float calcM;
    float epsilon = (float)0.01;
    for (size_t i = 0; i < steps; i++)
    {
        bpFileReader.BeginStep();
        auto varx = bpIn.InquireVariable<float>(varname[0]);
        auto vary = bpIn.InquireVariable<float>(varname[1]);
        auto varz = bpIn.InquireVariable<float>(varname[2]);
        auto varmag = bpIn.InquireVariable<uint8_t>(derivedname);

        bpFileReader.Get(varx, readUx);
        bpFileReader.Get(vary, readUy);
        bpFileReader.Get(varz, readUz);
        bpFileReader.Get(varmag, readMag);
        bpFileReader.EndStep();

        for (size_t ind = 0; ind < Nx * Ny * Nz; ++ind)
        {
            calcM = (float)sqrt(pow(readUx[ind], 2) + pow(readUy[ind], 2) + pow(readUz[ind], 2));
            EXPECT_TRUE(fabs(calcM - (float)readMag[ind]) < epsilon);
        }
    }
    bpFileReader.Close();
}

TEST(DerivedCorrectness, CurlCorrectnessTest)
{
    const size_t Nx = 25, Ny = 70, Nz = 13;
    float error_limit = 0.0000001f;

    // Application variable
    std::vector<float> simArray1(Nx * Ny * Nz);
    std::vector<float> simArray2(Nx * Ny * Nz);
    std::vector<float> simArray3(Nx * Ny * Nz);
    for (size_t i = 0; i < Nx; ++i)
    {
        for (size_t j = 0; j < Ny; ++j)
        {
            for (size_t k = 0; k < Nz; ++k)
            {
                size_t idx = (i * Ny * Nz) + (j * Nz) + k;
                float x = static_cast<float>(i);
                float y = static_cast<float>(j);
                float z = static_cast<float>(k);
                // Linear curl example
                simArray1[idx] = (6 * x * y) + (7 * z);
                simArray2[idx] = (4 * x * z) + powf(y, 2);
                simArray3[idx] = sqrtf(z) + (2 * x * y);
                /* Less linear example
                simArray1[idx] = sinf(z);
                simArray2[idx] = 4 * x;
                simArray3[idx] = powf(y, 2) * cosf(x);
                /* Nonlinear example
                simArray1[idx] = expf(2 * y) * sinf(x);
                simArray2[idx] = sqrtf(z + 1) * cosf(x);
                simArray3[idx] = powf(x, 2) * sinf(y) + (6 * z);
            }
        }
    }

    adios2::ADIOS adios;
    adios2::IO bpOut = adios.DeclareIO("BPWriteExpression");
    std::vector<std::string> varname = {"sim3/VX", "sim3/VY", "sim3/VZ"};
    std::string derivedname = "derived/curlV";

    auto VX = bpOut.DefineVariable<float>(varname[0], {Nx, Ny, Nz}, {0, 0, 0}, {Nx, Ny, Nz});
    auto VY = bpOut.DefineVariable<float>(varname[1], {Nx, Ny, Nz}, {0, 0, 0}, {Nx, Ny, Nz});
    auto VZ = bpOut.DefineVariable<float>(varname[2], {Nx, Ny, Nz}, {0, 0, 0}, {Nx, Ny, Nz});
    // clang-format off
    bpOut.DefineDerivedVariable(derivedname,
                                "Vx =" + varname[0] + " \n"
                                "Vy =" + varname[1] + " \n"
                                "Vz =" + varname[2] + " \n"
                                "curl(Vx,Vy,Vz)",
                                adios2::DerivedVarType::StoreData);
    // clang-format on
    std::string filename = "expCurl.bp";
    adios2::Engine bpFileWriter = bpOut.Open(filename, adios2::Mode::Write);

    bpFileWriter.BeginStep();
    bpFileWriter.Put(VX, simArray1.data());
    bpFileWriter.Put(VY, simArray2.data());
    bpFileWriter.Put(VZ, simArray3.data());
    bpFileWriter.EndStep();
    bpFileWriter.Close();

    adios2::IO bpIn = adios.DeclareIO("BPReadCurlExpression");
    adios2::Engine bpFileReader = bpIn.Open(filename, adios2::Mode::Read);

    std::vector<float> readVX;
    std::vector<float> readVY;
    std::vector<float> readVZ;

    std::vector<float> readCurl;

    std::vector<std::vector<float>> calcCurl;
    double sum_x = 0;
    double sum_y = 0;
    double sum_z = 0;
    bpFileReader.BeginStep();
    auto varVX = bpIn.InquireVariable<float>(varname[0]);
    auto varVY = bpIn.InquireVariable<float>(varname[1]);
    auto varVZ = bpIn.InquireVariable<float>(varname[2]);
    auto varCurl = bpIn.InquireVariable<float>(derivedname);

    bpFileReader.Get(varVX, readVX);
    bpFileReader.Get(varVY, readVY);
    bpFileReader.Get(varVZ, readVZ);
    bpFileReader.Get(varCurl, readCurl);
    bpFileReader.EndStep();

    float curl_x, curl_y, curl_z;
    float err_x, err_y, err_z;
    for (size_t i = 0; i < Nx; ++i)
    {
        for (size_t j = 0; j < Ny; ++j)
        {
            for (size_t k = 0; k < Nz; ++k)
            {
                size_t idx = (i * Ny * Nz) + (j * Nz) + k;
                float x = static_cast<float>(i);
                float y = static_cast<float>(j);
                float z = static_cast<float>(k);
                // Linear example
                curl_x = -(2 * x);
                curl_y = 7 - (2 * y);
                curl_z = (4 * z) - (6 * x);
                /* Less linear
                curl_x = 2 * y * cosf(x);
                curl_y = cosf(z) + (powf(y, 2) * sinf(x));
                curl_z = 4;
                */
/* Nonlinear example
curl_x = powf(x, 2) * cosf(y) - (cosf(x) / (2 * sqrtf(z + 1)));
curl_y = -2 * x * sinf(y);
curl_z = -sqrtf(z + 1) * sinf(x) - (2 * expf(2 * y) * sinf(x));
if (fabs(curl_x) < 1)
{
    err_x = fabs(curl_x - readCurl[3 * idx]) / (1 + fabs(curl_x));
}
else
{
    err_x = fabs(curl_x - readCurl[3 * idx]) / fabs(curl_x);
}
if (fabs(curl_y) < 1)
{
    err_y = fabs(curl_y - readCurl[3 * idx + 1]) / (1 + fabs(curl_y));
}
else
{
    err_y = fabs(curl_y - readCurl[3 * idx + 1]) / fabs(curl_y);
}
if (fabs(curl_z) < 1)
{
    err_z = fabs(curl_z - readCurl[3 * idx + 2]) / (1 + fabs(curl_z));
}
else
{
    err_z = fabs(curl_z - readCurl[3 * idx + 2]) / fabs(curl_z);
}
sum_x += err_x;
sum_y += err_y;
sum_z += err_z;
}
}
}
bpFileReader.Close();
EXPECT_LT((sum_x + sum_y + sum_z) / (3 * Nx * Ny * Nz), error_limit);
EXPECT_LT(sum_x / (Nx * Ny * Nz), error_limit);
EXPECT_LT(sum_y / (Nx * Ny * Nz), error_limit);
EXPECT_LT(sum_z / (Nx * Ny * Nz), error_limit);
}
*/
int main(int argc, char **argv)
{
    int result;
    ::testing::InitGoogleTest(&argc, argv);

    result = RUN_ALL_TESTS();

    return result;
}