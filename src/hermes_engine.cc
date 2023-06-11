//
// Created by lukemartinlogan on 3/28/23.
//

#include "coeus/hermes_engine.h"
#include <stdio.h>
#include <stdlib.h>
//#include <bucket.h>

namespace hapi = hermes::api;

namespace coeus {

/**
 * Construct the HermesEngine.
 *
 * This is a wrapper around Init_, which performs the main custom
 * initialization of the engine. The PluginEngineInterface will store
 * the "io" variable in the "m_IO" variable.
 * */
    HermesEngine::HermesEngine(adios2::core::IO &io,
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
        // Start the Hermes core Daemonqui
        // hermes->RunDaemon();
        // hapi::Hermes::RunDaemon();
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
    adios2::StepStatus HermesEngine::BeginStep(adios2::StepMode mode,
                                               const float timeoutSeconds) {
        std::cout << __func__ << std::endl;
    }

    size_t HermesEngine::CurrentStep() const {
        std::cout << __func__ << std::endl;
    }

    void HermesEngine::EndStep() {
        std::cout << __func__ << std::endl;
    }

  
  void DoPutDeferred_(adios2::core::Variable<T> &variable, const T *values) {
        std::cout << __func__ << std::endl;      
        hapi::Bucket bkt = HERMES->GetBucket(variable.Name());
        size_t blob_size = variable.SelectionSize() * sizeof(T);
        hapi::Context ctx;
        hermes::Blob blob;
        hermes::BlobId blob_id;
        memcpy(blob.data(), values , blob_size);
        bkt.Put(name, blob, blob_id, ctx);
    }
    
    void DoGetDeferred_(adios2::core::Variable<T> &variable, T *values) {
        std::cout << __func__ << std::endl;
        hapi::Bucket bkt = HERMES->GetBucket(variable.Name());
        size_t blob_size = variable.SelectionSize() * sizeof(T);
        hapi::Context ctx;
        hermes::BlobId blob_id;
        hermes::Blob blob;
        bkt.GetBlobId(name, blob_id);
        bkt.Get(blob_id, blob, ctx);
        memcpy(values, blob.data(), blob_size);
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
        fclose(fp_);
    }


/**
 * Initialize this engine.
 *
 * How do we know which files to open? The m_IO variable must contain
 * it somewhere. I don't know where yet.
 * */
    void HermesEngine::Init_() {
        std::cout << __func__ << std::endl;
        //hapi::Hermes::Create(hermes::HermesType::kClient);
        switch (m_OpenMode) {
            case adios2::Mode::Write: {
                fp_ = fopen(this->m_Name.c_str(), "w+");
            }
            case adios2::Mode::Read: {
                fp_ = fopen(this->m_Name.c_str(), "r+");
            }
            default: {
                return;
            }
        }
        if (!fp_) {
            perror("Failed to open input file");
            return;
        }
    }



}  // namespace coeus


/**
 * This is how ADIOS figures out where to dynamically load the engine.
 * */
extern "C" {

/** C wrapper to create engine */
coeus::HermesEngine *EngineCreate(adios2::core::IO &io,
                                  const std::string &name,
                                  const adios2::Mode mode,
                                  adios2::helper::Comm comm) {
    (void)mode;
    return new coeus::HermesEngine(io, name, mode, comm.Duplicate());
}

/** C wrapper to destroy engine */
void EngineDestroy(coeus::HermesEngine *obj) { delete obj; }
}
