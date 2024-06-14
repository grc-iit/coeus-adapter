//
// Created by hua on 6/11/24.
//

#include <vector>
#include <string>
#include <fstream>
#include <vector>
#include <string>
#include <comms/MPI.h>
#include <cereal/archives/binary.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <adios2/core/Variable.h>
#include <adios2/cxx11/Variable.h>
#include <adios2.h>
#ifndef COEUS_ADAPTER_ADIOSWRITE_H
#define COEUS_ADAPTER_ADIOSWRITE_H

template <typename T>
class Adios2Writer {
public:

    Adios2Writer(const std::string &engineType, const std::string &fileName, const std::string &variableName)
            : engineType_(engineType), fileName_(fileName), variableName_(variableName), adios_(adios2::ADIOS(MPI_COMM_WORLD)), io_(adios_.DeclareIO("OutputIO")) {
        // Set the engine type
        io_.SetEngine(engineType_);


    }

    void WriteData(const T *data, std::vector<size_t> shape, std::vector<size_t> start, std::vector<size_t> count) {
        // Open the engine to write data
        auto var_ = io_.DefineVariable<T>(variableName_, shape, start, count);
        //auto var_ = io_.DefineVariable<T>(variableName_, {adios2::LocalValueDim});
        adios2::Mode mode = adios2::Mode::Write;



        if (std::filesystem::exists(fileName_)) {
            mode = adios2::Mode::Append;
        }
        adios2::Engine writer = io_.Open(fileName_, mode);

        writer.BeginStep();
        // Perform the write operation
        writer.Put(var_, data);
        writer.EndStep();
        // Close the engine
        writer.Close();
    }

private:
    std::string engineType_;
    std::string fileName_;
    std::string variableName_;

    adios2::ADIOS adios_;
    adios2::IO io_;
    adios2::Variable<T> var_;
};


#endif //COEUS_ADAPTER_ADIOSWRITE_H
