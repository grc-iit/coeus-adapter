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
    : adios2::plugin::PluginEngineInterface(io, name, mode, comm.Duplicate()) {
  Init_();
  engine_logger->info("rank {} with name {} and mode {}", rank, name, adios2::ToString(mode));
}

/**
* Initialize this engine.
* */
void HermesEngine::Init_() {
  rank = m_Comm.Rank();

  comm_size = m_Comm.Size();

  hapi::Hermes::Create(hermes::HermesType::kClient);

  auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
#ifdef debug
  console_sink->set_level(spdlog::level::off);
#else
  console_sink->set_level(spdlog::level::warn);
#endif
  console_sink->set_pattern("[coeus engine] [%^%!%l%$] %v");

  auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("logs/coeus_engine.txt", true);
  file_sink->set_pattern("[coeus engine] [%^%!%l%$] %v");
  file_sink->set_level(spdlog::level::trace);

  spdlog::logger logger("debug_logger", {console_sink, file_sink});
  logger.set_level(spdlog::level::debug);

  engine_logger = std::make_shared<spdlog::logger>(logger);
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
    HermesPut("step", sizeof(int), &currentStep);
  }
  // Broadcast the updated value of currentStep from the
  // root process to all other processes
  m_Comm.Bcast(&currentStep, 1, 0);
}

void HermesEngine::LoadExistingVariables() {
  hapi::Bucket bkt_vars = HERMES->GetBucket("VariablesUsed");
  hapi::Context ctx_vars;
  std::vector<hermes::BlobId> blobIds = bkt_vars.GetContainedBlobIds();
  std::vector<hermes::Blob> blobs;
  for (const auto &blobId : blobIds) {
    hermes::Blob blob;
    bkt_vars.Get(blobId, blob, ctx_vars);
    const char *dataPtr = reinterpret_cast<const char *>(blob.data());
    std::string varName(dataPtr, blob.size());
    std::cout << "Variable is: " << varName << std::endl;
    listOfVars.push_back(varName);
  }
}

void HermesEngine::DefineVariable(VariableMetadata variableMetadata) {
  adios2::core::VariableBase *inquire_var = nullptr;
#define DEFINE_VARIABLE(T) \
          inquire_var = m_IO.InquireVariable<T>(variableMetadata.name);                              \
          if (adios2::helper::GetDataType<T>() == variableMetadata.getDataType()) { \
              if (!inquire_var) {                   \
                  m_IO.DefineVariable<T>( \
                          variableMetadata.name, \
                          variableMetadata.shape, \
                          variableMetadata.start, \
                          variableMetadata.count, \
                          variableMetadata.constantShape); \
              }                  \
          }
  ADIOS2_FOREACH_STDTYPE_1ARG(DEFINE_VARIABLE)
#undef DEFINE_VARIABLE
}

adios2::StepStatus HermesEngine::BeginStep(adios2::StepMode mode,
                                           const float timeoutSeconds) {
  // Increase currentStep and save it in Hermes
  IncrementCurrentStep();

  if (m_OpenMode == adios2::Mode::Read) {
    // Retrieve the metadata
    if (currentStep == 1) {
      LoadExistingVariables();
    }
    for (const std::string &varName : listOfVars) {
      std::string metadataName = "metadata_" + varName + std::to_string(currentStep) + "_rank" + std::to_string(rank);
      auto blob_metadata = HermesGet(metadataName, metadataName);
      VariableMetadata variableMetadata = MetadataSerializer::DeserializeMetadata(blob_metadata);
      DefineVariable(variableMetadata);
    }
  }
  engine_logger->info("rank {} on mode {}", rank, adios2::ToString(mode));
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
void HermesEngine::HermesPut(const std::string &bucket_name, size_t blob_size, T values) {
  hapi::Bucket bkt = HERMES->GetBucket(bucket_name);
  hapi::Context ctx;
  hermes::Blob blob_values(blob_size);
  hermes::BlobId blob_id_values;
  memcpy(blob_values.data(), values, blob_size);
  bkt.Put(bucket_name, blob_values, blob_id_values, ctx);
}

hermes::Blob HermesEngine::HermesGet(const std::string &bucket_name, const std::string &varName) {
  hapi::Bucket bkt = HERMES->GetBucket(bucket_name);
  hapi::Context ctx;
  hermes::Blob blob;
  hermes::BlobId blob_id_metadata;
  bkt.GetBlobId(varName, blob_id_metadata);
  bkt.Get(blob_id_metadata, blob, ctx);
  return blob;
}

template<typename T>
void HermesEngine::DoGetDeferred_(
    const adios2::core::Variable<T> &variable, T *values) {
  std::cout << __func__ << std::endl;

  // Retrieve the value of the variable in the current step
  std::string filename = variable.m_Name +
      std::to_string(currentStep) + "_rank" + std::to_string(rank);

  hapi::Bucket bkt = HERMES->GetBucket(filename);
  hapi::Context ctx;
  hermes::BlobId blob_id;
  hermes::Blob blob;
  bkt.GetBlobId(filename, blob_id);
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
  std::string bucket_name = variable.m_Name +
      std::to_string(currentStep) + "_rank" + std::to_string(rank);
  std::cout << "Bucket name is: " << bucket_name << std::endl;

  HermesPut(bucket_name, variable.SelectionSize() * sizeof(T), values);

  // Check if the value is already in the list
  auto it = std::find(listOfVars.begin(),
                      listOfVars.end(),
                      variable.m_Name);

  // Update the metadata bucket in hermes with the new variable
  if (it == listOfVars.end()) {
    listOfVars.push_back(variable.m_Name);
    HermesPut("VariablesUsed", variable.m_Name.size(), variable.m_Name.data());
  }

  if (bucket_name.compare(0, 4, "step") != 0) {
    // Store metadata in a separate metadata bucket
    std::string serializedMetadata =
        MetadataSerializer::SerializeMetadata<T>(variable);
    HermesPut("metadata_" + bucket_name,
              serializedMetadata.size(),
              serializedMetadata.data());
  }
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
  (void) mode;
  return new coeus::HermesEngine(io, name, mode, comm.Duplicate());
}

/** C wrapper to destroy engine */
void EngineDestroy(coeus::HermesEngine *obj) { delete obj; }
}

