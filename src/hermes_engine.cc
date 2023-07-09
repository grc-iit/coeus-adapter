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
        ++m_CurrentStep;
        adios2::StepStatus status = adios2::StepStatus::OK;
        return status;
    }


    size_t HermesEngine::CurrentStep() const {
        std::cout << __func__ << std::endl;
        return m_CurrentStep;
    }

    void HermesEngine::EndStep() {
        std::cout << __func__ << std::endl;
    }

    template<typename T>
    void HermesEngine::DoGetDeferred_(
            const adios2::core::Variable<T> &variable, T *values) {
        std::cout << __func__ << std::endl;
        hapi::Bucket bkt = HERMES->GetBucket(variable.m_Name);
        size_t blob_size = variable.SelectionSize() * sizeof(T);
        hapi::Context ctx;
        hermes::BlobId blob_id;
        hermes::Blob blob(blob_size);
        bkt.GetBlobId(variable.m_Name , blob_id);
        bkt.Get(blob_id, blob, ctx);
        memcpy(values, blob.data(), blob_size);
        #ifdef SAVE_TO_FILE
            // Save the information to a file
        std::string filename = "/tmp/tmp.5TUlmkfNlZ/data_Get.txt";
        std::ofstream outputFile(filename);
        if (outputFile.is_open()){
            for (size_t i = 0; i < blob_size / sizeof(T); ++i){
                outputFile << values[i] << " ";
            }
            outputFile.close();
            std::cout << "Data saved to file: " << filename << std::endl;
        } else {
            std::cout << "Unable to open file: " << filename << std::endl;
        }
        #endif
    }

    template<typename T>
    void HermesEngine::DoPutDeferred_(
            const adios2::core::Variable<T> &variable, const T *values) {
        std::cout << __func__ << std::endl;
        hapi::Bucket bkt = HERMES->GetBucket(variable.m_Name);
        size_t blob_size = variable.SelectionSize() * sizeof(T);
        hapi::Context ctx;
        hermes::Blob blob(blob_size);
        hermes::BlobId blob_id;
        memcpy(blob.data(), values , blob_size);
        std::cout << variable.m_Name << std::endl;
        std::cout << values << std::endl;

        bkt.Put(variable.m_Name, blob, blob_id, ctx);

        #ifdef SAVE_TO_FILE
            // Save the information to a file
        std::string filename = "/tmp/tmp.5TUlmkfNlZ/data_Put.txt";
        std::ofstream outputFile(filename);
        if (outputFile.is_open()){
            for (size_t i = 0; i < blob_size / sizeof(T); ++i){
                outputFile << values[i] << " ";
            }
            outputFile.close();
            std::cout << "Data saved to file: " << filename << std::endl;
        } else {
            std::cout << "Unable to open file: " << filename << std::endl;
        }
        #endif
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

