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

#include "coeus/HermesEngine.h"

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
  Hermes = std::make_unique<coeus::Hermes>();
  Init_();
  engine_logger->info("rank {} with name {} and mode {}",
                      rank, name, adios2::ToString(mode));
}

//Test initializer
HermesEngine::HermesEngine(std::unique_ptr<coeus::IHermes> h,
                           adios2::core::IO &io,
                           const std::string &name,
                           const adios2::Mode mode,
                           adios2::helper::Comm comm)
    : adios2::plugin::PluginEngineInterface(io, name, mode, comm.Duplicate()) {
  Hermes = std::move(h);
  Init_();
  engine_logger->info("rank {} with name {} and mode {}",
                      rank, name, adios2::ToString(mode));
}
/**
* Initialize this engine.
* */
void HermesEngine::Init_() {
  rank = m_Comm.Rank();
  comm_size = m_Comm.Size();

  auto opFile = m_IO.m_Parameters.find("OPFile");
  auto varFile = m_IO.m_Parameters.find("VarFile");

  if(!Hermes->connect()){
    throw coeus::common::ErrorException(HERMES_CONNECT_FAILED);
  }

  auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
  console_sink->set_level(spdlog::level::warn);
  console_sink->set_pattern("%^[Coeus engine] [%!:%# @ %s] [%l] %$ %v");
  console_sink->set_level(spdlog::level::warn);
  console_sink->set_pattern("[coeus engine] [%^%!%l%$] %v");

  auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
      "logs/engine_test.txt", true);
  file_sink->set_level(spdlog::level::trace);
  file_sink->set_pattern("%^[Coeus engine] [%!:%# @ %s] [%l] %$ %v");

  spdlog::logger logger("debug_logger", {console_sink, file_sink});
  logger.set_level(spdlog::level::debug);

  engine_logger = std::make_shared<spdlog::logger>(logger);
}

void HermesEngine::DoClose(const int transportIndex) {
  engine_logger->info("rank {}", rank);
  if (m_OpenMode == adios2::Mode::Write) {
    auto blob_name = "total_steps_" + m_IO.m_Name;
    auto bkt = Hermes->GetBucket("total_steps");
    bkt->Put(blob_name, sizeof(int), &currentStep);
  }
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
  }
  // Broadcast the updated value of currentStep from the
  // root process to all other processes
  m_Comm.Bcast(&currentStep, 1, 0);
}

bool HermesEngine::VariableMinMax(const adios2::core::VariableBase &Var,
                                  const size_t Step,
                                  adios2::MinMaxStruct &MinMax) {
  std::cout << __func__ << std::endl;

  // We initialize the min and max values
  InitElementMinMax(MinMax, Var.m_Type);

  std::string bucket_name = Var.m_Name + "_step_" + std::to_string(currentStep)
      + "_rank" + std::to_string(rank);

  // Obtain the blob from Hermes using the filename and variable name
  auto bkt = Hermes->GetBucket(bucket_name);
  hermes::Blob blob = bkt->Get(Var.m_Name);

#define DEFINE_VARIABLE(T) \
    if (adios2::helper::GetDataType<T>()  ==  Var.m_Type) { \
        size_t dataSize = blob.size() / sizeof(T);                               \
        const T *data = reinterpret_cast<const T *>(blob.data());               \
        for (size_t i = 0; i < dataSize; ++i) {               \
                void *elementPtr = const_cast<void *>(static_cast<const void *>(&data[i]));     \
                ApplyElementMinMax(MinMax, Var.m_Type, elementPtr);                   \
            }              \
    }
  ADIOS2_FOREACH_STDTYPE_1ARG(DEFINE_VARIABLE)
#undef DEFINE_VARIABLE
  return true;
}

void HermesEngine::InitElementMinMax(adios2::MinMaxStruct &MinMax,
                                     adios2::DataType Type) {
  switch (Type) {
    case adios2::DataType::None:break;
    case adios2::DataType::Char:
    case adios2::DataType::Int8:MinMax.MinUnion.field_int8 = std::numeric_limits<int8_t>::max();
      MinMax.MaxUnion.field_int8 = std::numeric_limits<int8_t>::min();
      break;
    case adios2::DataType::Int16:MinMax.MinUnion.field_int16 = std::numeric_limits<int16_t>::max();
      MinMax.MaxUnion.field_int16 = std::numeric_limits<int16_t>::min();
      break;
    case adios2::DataType::Int32:MinMax.MinUnion.field_int32 = std::numeric_limits<int32_t>::max();
      MinMax.MaxUnion.field_int32 = std::numeric_limits<int32_t>::min();
      break;
    case adios2::DataType::Int64:MinMax.MinUnion.field_int64 = std::numeric_limits<int64_t>::max();
      MinMax.MaxUnion.field_int64 = std::numeric_limits<int64_t>::min();
      break;
    case adios2::DataType::UInt8:MinMax.MinUnion.field_uint8 = std::numeric_limits<uint8_t>::max();
      MinMax.MaxUnion.field_uint8 = std::numeric_limits<uint8_t>::min();
      break;
    case adios2::DataType::UInt16:MinMax.MinUnion.field_uint16 = std::numeric_limits<uint16_t>::max();
      MinMax.MaxUnion.field_uint16 = std::numeric_limits<uint16_t>::min();
      break;
    case adios2::DataType::UInt32:MinMax.MinUnion.field_uint32 = std::numeric_limits<uint32_t>::max();
      MinMax.MaxUnion.field_uint32 = std::numeric_limits<uint32_t>::min();
      break;
    case adios2::DataType::UInt64:MinMax.MinUnion.field_uint64 = std::numeric_limits<uint64_t>::max();
      MinMax.MaxUnion.field_uint64 = std::numeric_limits<uint64_t>::min();
      break;
    case adios2::DataType::Float:MinMax.MinUnion.field_float = std::numeric_limits<float>::max();
      MinMax.MaxUnion.field_float = std::numeric_limits<float>::min();
      break;
    case adios2::DataType::Double:MinMax.MinUnion.field_double = std::numeric_limits<double>::max();
      MinMax.MaxUnion.field_double = std::numeric_limits<double>::min();
      break;
    case adios2::DataType::LongDouble:MinMax.MinUnion.field_ldouble = std::numeric_limits<long double>::max();
      MinMax.MaxUnion.field_ldouble = std::numeric_limits<long double>::min();
      break;
    case adios2::DataType::FloatComplex:
    case adios2::DataType::DoubleComplex:
    case adios2::DataType::String:
    case adios2::DataType::Struct:break;
  }
}

void HermesEngine::ApplyElementMinMax(adios2::MinMaxStruct &MinMax,
                                      adios2::DataType Type, void *Element) {

  switch (Type) {
    case adios2::DataType::None:break;
    case adios2::DataType::Char:
    case adios2::DataType::Int8:
      if (*reinterpret_cast<int8_t *>(Element) < MinMax.MinUnion.field_int8)
        MinMax.MinUnion.field_int8 = *(int8_t *) Element;
      if (*reinterpret_cast<int8_t *>(Element) > MinMax.MaxUnion.field_int8)
        MinMax.MaxUnion.field_int8 = *(int8_t *) Element;
      break;
    case adios2::DataType::Int16:
      if (*reinterpret_cast<int16_t *>(Element) < MinMax.MinUnion.field_int16)
        MinMax.MinUnion.field_int16 = *(int16_t *) Element;
      if (*reinterpret_cast<int16_t *>(Element) > MinMax.MaxUnion.field_int16)
        MinMax.MaxUnion.field_int16 = *(int16_t *) Element;
      break;
    case adios2::DataType::Int32:
      if (*reinterpret_cast<int32_t *>(Element) < MinMax.MinUnion.field_int32)
        MinMax.MinUnion.field_int32 = *(int32_t *) Element;
      if (*reinterpret_cast<int32_t *>(Element) > MinMax.MaxUnion.field_int32)
        MinMax.MaxUnion.field_int32 = *(int32_t *) Element;
      break;
    case adios2::DataType::Int64:
      if (*reinterpret_cast<int64_t *>(Element) < MinMax.MinUnion.field_int64)
        MinMax.MinUnion.field_int64 = *(int64_t *) Element;
      if (*reinterpret_cast<int64_t *>(Element) > MinMax.MaxUnion.field_int64)
        MinMax.MaxUnion.field_int64 = *(int64_t *) Element;
      break;
    case adios2::DataType::UInt8:
      if (*reinterpret_cast<uint8_t *>(Element) < MinMax.MinUnion.field_uint8)
        MinMax.MinUnion.field_uint8 = *(uint8_t *) Element;
      if (*reinterpret_cast<uint8_t *>(Element) > MinMax.MaxUnion.field_uint8)
        MinMax.MaxUnion.field_uint8 = *(uint8_t *) Element;
      break;
    case adios2::DataType::UInt16:
      if (*reinterpret_cast<uint16_t *>(Element) < MinMax.MinUnion.field_uint16)
        MinMax.MinUnion.field_uint16 = *(uint16_t *) Element;
      if (*reinterpret_cast<uint16_t *>(Element) > MinMax.MaxUnion.field_uint16)
        MinMax.MaxUnion.field_uint16 = *(uint16_t *) Element;
      break;
    case adios2::DataType::UInt32:
      if (*reinterpret_cast<uint32_t *>(Element) < MinMax.MinUnion.field_uint32)
        MinMax.MinUnion.field_uint32 = *(uint32_t *) Element;
      if (*reinterpret_cast<uint32_t *>(Element) > MinMax.MaxUnion.field_uint32)
        MinMax.MaxUnion.field_uint32 = *(uint32_t *) Element;
      break;
    case adios2::DataType::UInt64:
      if (*reinterpret_cast<uint64_t *>(Element) < MinMax.MinUnion.field_uint64)
        MinMax.MinUnion.field_uint64 = *(uint64_t *) Element;
      if (*reinterpret_cast<uint64_t *>(Element) > MinMax.MaxUnion.field_uint64)
        MinMax.MaxUnion.field_uint64 = *(uint64_t *) Element;
      break;
    case adios2::DataType::Float:
      if (*reinterpret_cast<float *>(Element) < MinMax.MinUnion.field_float)
        MinMax.MinUnion.field_float = *(float *) Element;
      if (*reinterpret_cast<float *>(Element) < MinMax.MinUnion.field_float)
        MinMax.MaxUnion.field_float = *(float *) Element;
      break;
    case adios2::DataType::Double:
      if (*reinterpret_cast<double *>(Element) < MinMax.MinUnion.field_double)
        MinMax.MinUnion.field_double = *(double *) Element;
      if (*reinterpret_cast<double *>(Element) > MinMax.MaxUnion.field_double)
        MinMax.MaxUnion.field_double = *(double *) Element;
      break;
    case adios2::DataType::LongDouble:
      if (*reinterpret_cast<long double *>(Element) < MinMax.MinUnion.field_ldouble)
        MinMax.MinUnion.field_ldouble = *(long double *) Element;
      if (*reinterpret_cast<long double *>(Element) > MinMax.MaxUnion.field_ldouble)
        MinMax.MaxUnion.field_ldouble = *(long double *) Element;
      break;
    case adios2::DataType::FloatComplex:
    case adios2::DataType::DoubleComplex:
    case adios2::DataType::String:
    case adios2::DataType::Struct:break;
  }
}

void HermesEngine::LoadMetadata() {

  std::string filename = "step_" + std::to_string(currentStep) +
      "_rank_" + std::to_string(rank);

  auto bkt = Hermes->GetBucket(filename);
  std::vector<hermes::BlobId> blobIds = bkt->GetContainedBlobIds();
  for (const auto &blobId : blobIds) {
    hermes::Blob blob = bkt->Get(blobId);
    VariableMetadata variableMetadata =
        MetadataSerializer::DeserializeMetadata(blob);
    listOfVars.push_back(bkt->GetBlobName(blobId));
    DefineVariable(variableMetadata);
  }
}

void HermesEngine::DefineVariable(VariableMetadata variableMetadata) {
  if (currentStep != 1) {
    // If the metadata is defined delete current value to update it
    m_IO.RemoveVariable(variableMetadata.name);
  }
#define DEFINE_VARIABLE(T) \
    if (adios2::helper::GetDataType<T>() == variableMetadata.getDataType()) { \
         adios2::core::Variable<T> *variable = &(m_IO.DefineVariable<T>( \
                      variableMetadata.name, \
                      variableMetadata.shape, \
                      variableMetadata.start, \
                      variableMetadata.count, \
                      variableMetadata.constantShape));                         \
         variable->m_AvailableStepsCount = 1;                                   \
         variable->m_SingleValue = false;                                       \
         variable->m_Min = std::numeric_limits<T>::max();                       \
         variable->m_Max = std::numeric_limits<T>::min();                       \
         variable->m_Engine = this;                                             \
          }
  ADIOS2_FOREACH_STDTYPE_1ARG(DEFINE_VARIABLE)
#undef DEFINE_VARIABLE
}

adios2::StepStatus HermesEngine::BeginStep(adios2::StepMode mode,
                                           const float timeoutSeconds) {
  std::cout << __func__ << std::endl;
  IncrementCurrentStep();
  if (m_OpenMode == adios2::Mode::Read) {
    auto bkt = Hermes->GetBucket("total_steps");
    hermes::Blob blob = bkt->Get("total_steps_" + m_IO.m_Name);
    total_steps = *reinterpret_cast<const int *>(blob.data());
    if (currentStep > total_steps) {
      return adios2::StepStatus::EndOfStream;
    }
    LoadMetadata();
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
void HermesEngine::DoGetDeferred_(
    const adios2::core::Variable<T> &variable, T *values) {
  std::cout << __func__ << std::endl;

  // Retrieve the value of the variable in the current step
  std::string bucket_name = variable.m_Name + "_step_" +
      std::to_string(currentStep) + "_rank" + std::to_string(rank);
  auto bkt = Hermes->GetBucket(bucket_name);
  auto blob = bkt->Get(variable.m_Name);
  memcpy(values, blob.data(), blob.size());
}

template<typename T>
void HermesEngine::DoPutDeferred_(
    const adios2::core::Variable<T> &variable, const T *values) {
  std::cout << __func__ << std::endl;

  // Create a bucket with the associated step and process rank
  std::string bucket_name = variable.m_Name + "_step_" +
      std::to_string(currentStep) + "_rank" + std::to_string(rank);

  auto bkt = Hermes->GetBucket(bucket_name);
  auto status = bkt->Put(variable.variable.m_Name,
                       variable.SelectionSize() * sizeof(T), values);

  // Check if the value is already in the list
  auto it = std::find(listOfVars.begin(),
                      listOfVars.end(),
                      variable.m_Name);

  // Update the metadata bucket in hermes with the new variable
  if (it == listOfVars.end()) {
    std::string bucket_name_metadata = "step_" + std::to_string(currentStep) +
        "_rank_" + std::to_string(rank);
    listOfVars.push_back(variable.m_Name);

    std::string serializedMetadata =
        MetadataSerializer::SerializeMetadata<T>(variable);

    auto bkt = Hermes->GetBucket(bucket_name_metadata);
    auto status = bkt->Put(variable.m_Name, serializedMetadata.size(), serializedMetadata.data());
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

