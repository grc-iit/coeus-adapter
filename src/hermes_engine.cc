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
#include "spdlog/spdlog.h"
#include <stdio.h>
#include <stdlib.h>

#define SAVE_TO_FILE

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

    adios2::StepStatus HermesEngine::BeginStep(adios2::StepMode mode,
                                               const float timeoutSeconds) {
        std::cout << __func__ << std::endl;
        m_Engine = this;
        // Increase currentStep and save it in Hermes
        if (rank == 0) {
          currentStep++;
          hapi::Bucket bkt = HERMES->GetBucket("step");
          size_t blob_size = sizeof(int);
          hapi::Context ctx;
          hermes::Blob blob(blob_size);
          hermes::BlobId blob_id;
          // currentStep = static_cast<int>(currentStep) + 1;
          memcpy(blob.data(), &currentStep, blob_size);
          bkt.Put("step", blob, blob_id, ctx);
        }
        // Broadcast the updated value of currentStep from the
        // root process to all other processes
        m_Comm.Bcast(&currentStep, 1, 0);
        std::cout << "We are at step: " << currentStep << std::endl;

        // Retrieve filename HERE (we are using "metadata_myVar")
        // ........
        // ........

        if (this->m_OpenMode == adios2::Mode::Read) {
            // Retrieve the metadata
            std::string metadataName = "metadata_myVar" + std::to_string(currentStep) + "_rank" + std::to_string(rank);
            hapi::Bucket bkt_metadata = HERMES->GetBucket(metadataName);
            hapi::Context ctx_metadata;
            hermes::Blob blob_metadata;
            hermes::BlobId blob_id_metadata;
            bkt_metadata.GetBlobId(metadataName, blob_id_metadata);
            bkt_metadata.Get(blob_id_metadata, blob_metadata, ctx_metadata);

            VariableMetadata variableMetadata = MetadataSerializer::DeserializeMetadata(blob_metadata);
            // Now we can access the variable metadata
            std::string variableName = variableMetadata.name;
            std::vector<size_t> variableShape = variableMetadata.shape;
            std::vector<size_t> variableStart = variableMetadata.start;
            std::vector<size_t> variableCount = variableMetadata.count;
            bool variableConstantShape = variableMetadata.constantShape;

            //adios2::DataType dataType = variableMetadata.getDataType();

            adios2::core::Variable<double> *inquire_var = m_IO.InquireVariable<double>(variableName);
            if(!inquire_var) {
                std::cout << "!inquire_var" << std::endl;
                m_IO.DefineVariable<double>(variableName,variableShape,
                                            variableStart, variableCount,
                                            variableConstantShape);
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

        std::cout << "File name is: " << filename << std::endl;
        hapi::Bucket bkt = HERMES->GetBucket(filename);
        hapi::Context ctx;
        hermes::BlobId blob_id;
        hermes::Blob blob;
        bkt.GetBlobId(filename , blob_id);
        bkt.Get(blob_id, blob, ctx);
        memcpy(values, blob.data(), blob.size());
    }


   /* template<typename T>
    void getMetadataAndUpload(adios2::core::Engine *engine, int currentStep,
                              int rank, const adios2::core::Variable<T> &variable) {
        std::cout << __func__ << " Seaching for- Step: " << currentStep << ", Rank: " << rank  << ", Putting: " << variable.m_Name << std::endl;
        // Get the bucket with the associated step and process rank
        std::string filename = variable.m_Name + std::to_string(currentStep) + "_rank" + std::to_string(rank);
        hapi::Bucket bkt = HERMES->GetBucket(filename);
        hapi::Context ctx;

        if (filename.compare(0, 4, "step") != 0) {
            std::string metadataName = "metadata_" + filename;
            hapi::Bucket bkt_metadata = HERMES->GetBucket(metadataName);

            hermes::Blob blob_metadata;
            hermes::BlobId blob_id_metadata;
            bkt.GetBlobId(metadataName , blob_id_metadata);
            bkt_metadata.Get(blob_id_metadata, blob_metadata, ctx);
            VariableMetadata deserializedMetadata = MetadataSerializer::DeserializeMetadata(blob_metadata);

            std::vector<size_t> VecShape;
            std::vector<size_t> VecStart;
            std::vector<size_t> VecCount;

            hermes::Blob blob_shape;
            hermes::BlobId blob_id_shape;
            bkt.GetBlobId(metadataName + "_shape", blob_id_shape);
            bkt_metadata.Get(blob_id_shape, blob_shape, ctx);
            VecShape.assign(reinterpret_cast<const size_t*>(blob_shape.data()), reinterpret_cast<const size_t*>(blob_shape.data() + blob_shape.size()));

            hermes::Blob blob_start;
            hermes::BlobId blob_id_start;
            bkt.GetBlobId(metadataName "_start" , blob_id_start);
            bkt_metadata.Get(blob_id_start, blob_start, ctx);
            VecStart.assign(reinterpret_cast<const size_t*>(blob_start.data()), reinterpret_cast<const size_t*>(blob_start.data() + blob_start.size()));

            hermes::Blob blob_count;
            hermes::BlobId blob_id_count;
            bkt.GetBlobId(metadataName "_count" , blob_id_count);
            bkt_metadata.Get(blob_id_count, blob_count, ctx);
            VecCount.assign(reinterpret_cast<const size_t*>(blob_count.data()), reinterpret_cast<const size_t*>(blob_count.data() + blob_count.size()));

            //Now that we have the metadata in the system we need to upload it to the IO
            adios2::core::VariableStruct *metadata = engine->m_IO.DefineVariable<T>(
                    deserializedMetadata.name, VecShape, VecStart, VecCount);
            engine->RegisterCreatedVariable(metadata);
        }
    }*/


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

        if (filename.compare(0, 4, "step") != 0) {
            // Store metadata in a separate metadata bucket
            std::string metadataName = "metadata_" + filename;
            std::cout << "Metadata Bucket name: " << metadataName << std::endl;

///////////////// PRINT METADATA ///////////////////////////////
            std::cout << "Shape: ";
            for (const auto& shape : variable.m_Shape) {
                std::cout << shape << " ";
            }
            std::cout << std::endl;
            std::cout << "Start: ";
            for (const auto& start : variable.m_Start) {
                std::cout << start << " ";
            }
            std::cout << std::endl;
            std::cout << "Count: ";
            for (const auto& count : variable.m_Count) {
                std::cout << count << " ";
            }
            std::cout << std::endl;
////////////////////////////////////////////////////////////////
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

            /*size_t blob_shape_size = variable.m_Shape.size();
            hermes::Blob blob_shape(blob_shape_size);
            hermes::BlobId blob_id_shape;
            memcpy(blob_shape.data(), &variable.m_Shape, blob_shape_size);
            bkt_metadata.Put(metadataName + "_shape", blob_shape, blob_id_shape, ctx);

            size_t blob_start_size = variable.m_Start.size();
            hermes::Blob blob_start(blob_start_size);
            hermes::BlobId blob_id_start;
            memcpy(blob_start.data(), &variable.m_Start, blob_start_size);
            bkt_metadata.Put(metadataName + "_start", blob_start, blob_id_start, ctx);

            size_t blob_count_size = variable.m_Count.size();
            hermes::Blob blob_count(blob_count_size);
            hermes::BlobId blob_id_count;
            memcpy(blob_count.data(), &variable.m_Count, blob_count_size);
            bkt_metadata.Put(metadataName + "_count", blob_count, blob_id_count, ctx);*/
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

