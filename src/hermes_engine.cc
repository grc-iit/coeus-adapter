/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Distributed under BSD 3-Clause license.                                   *
 * Copyright by the Illinois Institute of Technology.                        *
 * All rights reserved.                                                      *
 *                                                                           *
 * This file is part of Coeus-adapter. The full Coeus-adapter copyright      *
 * notice, including terms governing use, modification, and redistribution,  *
 * is contained in the COPYING file, which can be found at the top directory.*
 * If you do not have access to the file, you may request a copy             *
 * from scslab@iit.edu.                                                      *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "coeus/hermes_engine.h"
#include <stdio.h>
#include <stdlib.h>
#include "spdlog/spdlog.h"

namespace hapi = hermes::api;

namespace coeus {

/**
 * Construct the HermesEngine.
 *
 * This is a wrapper around Init_, which performs the main custom
 * initialization of the engine. The PluginEngineInterface will store
 * the "io" variable in the "m_IO" variable.
 * */
    HermesEngine::HermesEngine(adios2::core::IO &io,//NOLINT
                               const std::string &name,
                               const adios2::Mode mode,
                               adios2::helper::Comm comm)
            : adios2::plugin::PluginEngineInterface(io,
                                                    name,
                                                    mode,
                                                    comm.Duplicate()) {
        // Create object hermes
        // NOTE(llogan): name = params["PluginName"]
        std::cout << __func__ << std::endl;
        Init_();

        rank = m_Comm.Rank();
    }

/**
 * Destruct the HermesEngine.
 * */
    HermesEngine::~HermesEngine() {
    }

/**
 * In ADIOS2, a "step" refers to a unit of data that is written or read.
 * Each step represents a snapshot of the data at a specific time, and can
 * be thought of as a frame in a video or a snapshot of a simulation.
 * */

/**
    * Define the beginning of a step. A step is typically the offset from
    * the beginning of a file. It is measured as a size_t. -- Should we compute this every put or get call
    *
    * Logically, a "step" represents a snapshot of the data at a specific time,
    * and can be thought of as a frame in a video or a snapshot of a simulation.
* */

    void HermesEngine::IncrementCurrentStep() {
        if (rank == 0) {
            currentStep++;
            hapi::Bucket bkt = HERMES->GetBucket("step");
            size_t blob_size = sizeof(int);
            hapi::Context ctx;
            hermes::Blob blob(blob_size);
            hermes::BlobId blob_id;
            memcpy(blob.data(), &currentStep, blob_size);
            bkt.Put("step", blob, blob_id, ctx);
        }
    }

    void HermesEngine::LoadExistingVariables() {
        hapi::Bucket bkt_vars = HERMES->GetBucket("VariablesUsed");
        hapi::Context ctx_vars;
        std::vector<hermes::BlobId> blobIds = bkt_vars.GetContainedBlobIds();
        std::vector<hermes::Blob> blobs;
        for (const auto& blobId : blobIds) {
            hermes::Blob blob;
            bkt_vars.Get(blobId, blob, ctx_vars);
            const char* dataPtr = reinterpret_cast<const char*>(blob.data());
            std::string varName(dataPtr, blob.size());
            std::cout << "Variable is: " << varName << std::endl;
            listOfVars.push_back(varName);
        }
    }


    void HermesEngine::DefineVariableIfNeeded(const std::string& varName) {
        std::string metadataName = "metadata_" + varName +
                std::to_string(currentStep) + "_rank" + std::to_string(rank);
        hapi::Bucket bkt_metadata = HERMES->GetBucket(metadataName);
        hapi::Context ctx_metadata;
        hermes::Blob blob_metadata;
        hermes::BlobId blob_id_metadata;
        bkt_metadata.GetBlobId(metadataName, blob_id_metadata);
        bkt_metadata.Get(blob_id_metadata, blob_metadata, ctx_metadata);

        VariableMetadata variableMetadata =
                MetadataSerializer::DeserializeMetadata(blob_metadata);
        adios2::core::VariableBase* inquire_var = nullptr;
        switch (variableMetadata.getDataType()) {
            case adios2::DataType::Int8:
                inquire_var = m_IO.InquireVariable<int8_t>(varName);
                if (!inquire_var) {
                    m_IO.DefineVariable<int8_t>(
                            varName,
                            variableMetadata.shape,
                            variableMetadata.start,
                            variableMetadata.count,
                            variableMetadata.constantShape);
                }
                break;
            case adios2::DataType::Int16:
                inquire_var = m_IO.InquireVariable<int16_t>(varName);
                if (!inquire_var) {
                    m_IO.DefineVariable<int16_t>
                            (varName,
                             variableMetadata.shape,
                             variableMetadata.start,
                             variableMetadata.count,
                             variableMetadata.constantShape);
                }
                break;
            case adios2::DataType::Int32:
                inquire_var = m_IO.InquireVariable<int32_t>(varName);
                if (!inquire_var) {
                    m_IO.DefineVariable<int32_t>
                            (varName,
                             variableMetadata.shape,
                             variableMetadata.start,
                             variableMetadata.count,
                             variableMetadata.constantShape);
                }
                break;
            case adios2::DataType::Int64:
                inquire_var = m_IO.InquireVariable<int64_t>(varName);
                if (!inquire_var) {
                    m_IO.DefineVariable<int64_t>
                            (varName,
                             variableMetadata.shape,
                             variableMetadata.start,
                             variableMetadata.count,
                             variableMetadata.constantShape);
                }
                break;
            case adios2::DataType::UInt8:
                inquire_var = m_IO.InquireVariable<uint8_t>(varName);
                if (!inquire_var) {
                    m_IO.DefineVariable<uint8_t>
                            (varName,
                             variableMetadata.shape,
                             variableMetadata.start,
                             variableMetadata.count,
                             variableMetadata.constantShape);
                }
                break;
            case adios2::DataType::UInt16:
                inquire_var = m_IO.InquireVariable<uint16_t>(varName);
                if (!inquire_var) {
                    m_IO.DefineVariable<uint16_t>
                            (varName,
                              variableMetadata.shape,
                              variableMetadata.start,
                              variableMetadata.count,
                              variableMetadata.constantShape);
                }
                break;
            case adios2::DataType::UInt32:
                inquire_var = m_IO.InquireVariable<uint32_t>(varName);
                if (!inquire_var) {
                    m_IO.DefineVariable<uint32_t>
                            (varName,
                              variableMetadata.shape,
                              variableMetadata.start,
                              variableMetadata.count,
                              variableMetadata.constantShape);
                }
                break;
            case adios2::DataType::UInt64:
                inquire_var = m_IO.InquireVariable<uint64_t>(varName);
                if (!inquire_var) {
                    m_IO.DefineVariable<uint64_t>
                            (varName,
                              variableMetadata.shape,
                              variableMetadata.start,
                              variableMetadata.count,
                              variableMetadata.constantShape);
                }
                break;
            case adios2::DataType::Float:
                inquire_var = m_IO.InquireVariable<float>(varName);
                if (!inquire_var) {
                    m_IO.DefineVariable<float>
                            (varName,
                               variableMetadata.shape,
                               variableMetadata.start,
                               variableMetadata.count,
                               variableMetadata.constantShape);
                }
                break;
            case adios2::DataType::Double:
                inquire_var = m_IO.InquireVariable<double>(varName);
                if (!inquire_var) {
                    m_IO.DefineVariable<double>
                            (varName,
                                variableMetadata.shape,
                                variableMetadata.start,
                                variableMetadata.count,
                                variableMetadata.constantShape);
                }
                break;
            case adios2::DataType::LongDouble:
                inquire_var = m_IO.InquireVariable<long double>(varName);
                if (!inquire_var) {
                    m_IO.DefineVariable<long double>
                            (varName,
                             variableMetadata.shape,
                             variableMetadata.start,
                             variableMetadata.count,
                             variableMetadata.constantShape);
                }
                break;
            case adios2::DataType::FloatComplex:
                inquire_var =
                        m_IO.InquireVariable<std::complex<float>>(varName);
                if (!inquire_var) {
                    m_IO.DefineVariable<std::complex<float>>
                            (varName,
                            variableMetadata.shape,
                            variableMetadata.start,
                            variableMetadata.count,
                            variableMetadata.constantShape);
                }
                break;
            case adios2::DataType::DoubleComplex:
                inquire_var =
                        m_IO.InquireVariable<std::complex<double>>(varName);
                if (!inquire_var) {
                    m_IO.DefineVariable<std::complex<double>>
                            (varName,
                            variableMetadata.shape,
                            variableMetadata.start,
                            variableMetadata.count,
                            variableMetadata.constantShape);
                }
                break;
            case adios2::DataType::String:
                inquire_var = m_IO.InquireVariable<std::string>(varName);
                if (!inquire_var) {
                    m_IO.DefineVariable<std::string>
                            (varName,
                             variableMetadata.shape,
                             variableMetadata.start,
                             variableMetadata.count,
                             variableMetadata.constantShape);
                }
                break;
            case adios2::DataType::Char:
                inquire_var = m_IO.InquireVariable<char>(varName);
                if (!inquire_var) {
                    m_IO.DefineVariable<char>
                            (varName,
                              variableMetadata.shape,
                              variableMetadata.start,
                              variableMetadata.count,
                              variableMetadata.constantShape);
                }
                break;
            default:
                break;
        }
    }


    adios2::StepStatus HermesEngine::BeginStep(adios2::StepMode mode,
                                               const float timeoutSeconds) {
        std::cout << __func__ << std::endl;
        m_Engine = this;
        // Increase currentStep and save it in Hermes
        IncrementCurrentStep();

        // Broadcast the updated value of currentStep from the
        // root process to all other processes
        m_Comm.Bcast(&currentStep, 1, 0);
        std::cout << "We are at step: " << currentStep << std::endl;

        if (this->m_OpenMode == adios2::Mode::Read) {
            // Retrieve the metadata
            if (currentStep == 1) {
                LoadExistingVariables();
            }
            for (std::string varName : listOfVars) {
                DefineVariableIfNeeded(varName);
            }
        }
        return adios2::StepStatus::OK;
    }

    size_t HermesEngine::CurrentStep() const {
        std::cout << __func__ << std::endl;
        return currentStep;
    }

    void HermesEngine::EndStep() {
        std::cout << __func__ << std::endl;
    }


    template<typename T>
    void HermesEngine::DoGetDeferred_(
            const adios2::core::Variable<T> &variable, T *values) {
        std::cout << __func__ << std::endl;

        // Retrieve the value of the variable in the current step
        std::string filename = variable.m_Name +
                std::to_string(currentStep) + "_rank" +
                std::to_string(rank);

        hapi::Bucket bkt = HERMES->GetBucket(filename);
        hapi::Context ctx;
        hermes::BlobId blob_id;
        hermes::Blob blob;
        bkt.GetBlobId(filename , blob_id);
        bkt.Get(blob_id, blob, ctx);
        memcpy(values, blob.data(), blob.size());
    }



    template<typename T>
    void HermesEngine::DoPutDeferred_(
            const adios2::core::Variable<T> &variable, const T *values) {
        std::cout << __func__ << " - Step: " << currentStep
        << ", Rank: " << rank << ", Putting: " << variable.m_Name
        << " with value: " << *values << std::endl;

        // Create a bucket with the associated step and process rank
        std::string filename = variable.m_Name +
                std::to_string(currentStep) + "_rank" + std::to_string(rank);
        std::cout << "Bucket name is: " << filename << std::endl;

        hapi::Bucket bkt = HERMES->GetBucket(filename);
        size_t blob_size = variable.SelectionSize() * sizeof(T);
        hapi::Context ctx;
        hermes::Blob blob_values(blob_size);
        hermes::BlobId blob_id_values;
        memcpy(blob_values.data(), values, blob_size);
        bkt.Put(filename, blob_values, blob_id_values, ctx);

        // Check if the value is already in the list
        auto it = std::find(listOfVars.begin(),
                            listOfVars.end(),
                            variable.m_Name);

        if (it == listOfVars.end()) {
            listOfVars.push_back(variable.m_Name);
            // Update the metadata bucket in hermes with the new variable
            hapi::Bucket bkt_vars = HERMES->GetBucket("VariablesUsed");
            hapi::Context ctx_vars;
            hermes::Blob blob_vars(variable.m_Name.size());
            hermes::BlobId blob_id_vars;
            memcpy(blob_vars.data(),
                   variable.m_Name.data(),
                   variable.m_Name.size());
            bkt_vars.Put(variable.m_Name, blob_vars, blob_id_vars, ctx);
        }

        if (filename.compare(0, 4, "step") != 0) {
            // Store metadata in a separate metadata bucket
            std::string metadataName = "metadata_" + filename;
            std::string serializedMetadata =
                    MetadataSerializer::SerializeMetadata<T>(variable);
            hapi::Bucket bkt_metadata = HERMES->GetBucket(metadataName);
            size_t blob_metadata_size = serializedMetadata.size();
            hermes::Blob blob_metadata(blob_metadata_size);
            hermes::BlobId blob_id_metadata;
            memcpy(blob_metadata.data(),
                   serializedMetadata.data(), blob_metadata_size);
            bkt_metadata.Put(metadataName,
                             blob_metadata, blob_id_metadata, ctx);
        }
    }

    template<typename T>
    void DoPutSync_(const adios2::core::Variable<T> &variable,
                    const T *values) {
        std::cout << __func__ << std::endl;
    }

    template<typename T>
    void DoGetSync_(const adios2::core::Variable<T> &variable, T *values) {
        std::cout << __func__ << std::endl;
    }

    void HermesEngine::PerformPuts() {
        std::cout << __func__ << std::endl;
    }

    void HermesEngine::PerformGets() {
        std::cout << __func__ << std::endl;
    }

/** Close a particular transport */
    void HermesEngine::DoClose(const int transportIndex) {
        std::cout << __func__ << std::endl;
    }

/**
 * Initialize this engine.
 *
 * How do we know which files to open? The m_IO variable must contain
 * it somewhere. I don't know where yet.
 * */
    void HermesEngine::Init_() {
        std::cout << __func__ << std::endl;
        hapi::Hermes::Create(hermes::HermesType::kClient);
    }

}  // namespace coeus


/**
 * This is how ADIOS figures out where to dynamically load the engine.
 * */
extern "C" {

/** C wrapper to create engine */
coeus::HermesEngine *EngineCreate(adios2::core::IO &io,//NOLINT
                                  const std::string &name,
                                  const adios2::Mode mode,
                                  adios2::helper::Comm comm) {
    (void)mode;
    return new coeus::HermesEngine(io, name, mode, comm.Duplicate());
}

/** C wrapper to destroy engine */
void EngineDestroy(coeus::HermesEngine *obj) { delete obj; }
}

