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

  m_Engine = this;

  rank = m_Comm.Rank();

  comm_size = m_Comm.Size();

  hapi::Hermes::Create(hermes::HermesType::kClient);

  auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
  console_sink->set_level(spdlog::level::warn);
  console_sink->set_pattern("%^[Coeus engine] [%!:%# @ %s] [%l] %$ %v");
#ifdef debug
  console_sink->set_level(spdlog::level::off);
#else
  console_sink->set_level(spdlog::level::warn);
#endif
  console_sink->set_pattern("[coeus engine] [%^%!%l%$] %v");

  auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("logs/engine_test.txt", true);
  file_sink->set_level(spdlog::level::trace);
  file_sink->set_pattern("%^[Coeus engine] [%!:%# @ %s] [%l] %$ %v");

  auto mix_log = std::make_shared<spdlog::logger>(spdlog::logger("debug_logger", {console_sink, file_sink}));
  mix_log->set_level(spdlog::level::debug);

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
    HermesPut("step","step",sizeof(int), &currentStep);
  }
  // Broadcast the updated value of currentStep from the
  // root process to all other processes
  m_Comm.Bcast(&currentStep, 1, 0);
}

bool HermesEngine::VariableMinMax(const adios2::core::VariableBase &Var, const size_t Step,
                                      adios2::MinMaxStruct &MinMax) {
    std::cout << __func__ << std::endl;

    //We should do this for all the types?
    MinMax.MinUnion.field_double = std::numeric_limits<double>::max();

    // Idea:
    // Get the corresponding hermes data with the name, we can obtain this with the Var.m_Name
    // save the corresponding data of the variable passed as input to the MiMax structure
    //for (size_t step = 1; step <= currentStep; ++step) {
    std::string filename = Var.m_Name + "_step_" + std::to_string(currentStep)
            + "_rank" + std::to_string(rank);

// Obtain the blob from Hermes using the filename and variable name
    hermes::Blob blob = HermesGet(filename, Var.m_Name);

// Calculate the number of elements in the blob (assuming the values are doubles)
    size_t dataSize = blob.size() / sizeof(double);

// Create a pointer to the double data inside the blob
    const double *doubleData = reinterpret_cast<const double *>(blob.data());
// Iterate through the data and call ApplyElementMinMax for each element
    for (size_t i = 0; i < dataSize; ++i) {
        void *elementPtr = const_cast<void *>(static_cast<const void *>(&doubleData[i]));
        ApplyElementMinMax(MinMax, Var.m_Type, elementPtr);
    }

    //}
    return true;
}

void HermesEngine::ApplyElementMinMax(adios2::MinMaxStruct &MinMax,
                                      adios2::DataType Type, void *Element) {
        switch (Type)
        {
            case adios2::DataType::None:
                std::cout << "In case 20"<< std::endl;
                break;
            case adios2::DataType::Char:
                std::cout << "In case 11"<< std::endl;
            case adios2::DataType::Int8:
                std::cout << "In case 10"<< std::endl;
                if (*(int8_t *)Element < MinMax.MinUnion.field_int8)
                    MinMax.MinUnion.field_int8 = *(int8_t *)Element;
                if (*(int8_t *)Element > MinMax.MaxUnion.field_int8)
                    MinMax.MaxUnion.field_int8 = *(int8_t *)Element;
                break;
            case adios2::DataType::Int16:
                std::cout << "In case 8"<< std::endl;
                if (*(int16_t *)Element < MinMax.MinUnion.field_int16)
                    MinMax.MinUnion.field_int16 = *(int16_t *)Element;
                if (*(int16_t *)Element > MinMax.MaxUnion.field_int16)
                    MinMax.MaxUnion.field_int16 = *(int16_t *)Element;
                break;
            case adios2::DataType::Int32:
                if (*(int32_t *)Element < MinMax.MinUnion.field_int32)
                    MinMax.MinUnion.field_int32 = *(int32_t *)Element;
                if (*(int32_t *)Element > MinMax.MaxUnion.field_int32)
                    MinMax.MaxUnion.field_int32 = *(int32_t *)Element;
                break;
            case adios2::DataType::Int64:
                if (*(int64_t *)Element < MinMax.MinUnion.field_int64)
                    MinMax.MinUnion.field_int64 = *(int64_t *)Element;
                if (*(int64_t *)Element > MinMax.MaxUnion.field_int64)
                    MinMax.MaxUnion.field_int64 = *(int64_t *)Element;
                break;
            case adios2::DataType::UInt8:
                if (*(uint8_t *)Element < MinMax.MinUnion.field_uint8)
                    MinMax.MinUnion.field_uint8 = *(uint8_t *)Element;
                if (*(uint8_t *)Element > MinMax.MaxUnion.field_uint8)
                    MinMax.MaxUnion.field_uint8 = *(uint8_t *)Element;
                break;
            case adios2::DataType::UInt16:
                if (*(uint16_t *)Element < MinMax.MinUnion.field_uint16)
                    MinMax.MinUnion.field_uint16 = *(uint16_t *)Element;
                if (*(uint16_t *)Element > MinMax.MaxUnion.field_uint16)
                    MinMax.MaxUnion.field_uint16 = *(uint16_t *)Element;
                break;
            case adios2::DataType::UInt32:
                if (*(uint32_t *)Element < MinMax.MinUnion.field_uint32)
                    MinMax.MinUnion.field_uint32 = *(uint32_t *)Element;
                if (*(uint32_t *)Element > MinMax.MaxUnion.field_uint32)
                    MinMax.MaxUnion.field_uint32 = *(uint32_t *)Element;
                break;
            case adios2::DataType::UInt64:
                std::cout << "In case 3"<< std::endl;
                if (*(uint64_t *)Element < MinMax.MinUnion.field_uint64)
                    MinMax.MinUnion.field_uint64 = *(uint64_t *)Element;
                if (*(uint64_t *)Element > MinMax.MaxUnion.field_uint64)
                    MinMax.MaxUnion.field_uint64 = *(uint64_t *)Element;
                break;
            case adios2::DataType::Float:
                std::cout << "In case 2"<< std::endl;
                if (*(float *)Element < MinMax.MinUnion.field_float)
                    MinMax.MinUnion.field_float = *(float *)Element;
                if (*(float *)Element > MinMax.MaxUnion.field_float)
                    MinMax.MaxUnion.field_float = *(float *)Element;
                break;
            case adios2::DataType::Double:
                if (*(double *)Element < MinMax.MinUnion.field_double)
                    MinMax.MinUnion.field_double = *(double *)Element;
                if (*(double *)Element > MinMax.MaxUnion.field_double)
                    MinMax.MaxUnion.field_double = *(double *)Element;
                break;
            case adios2::DataType::LongDouble:
                if (*(long double *)Element < MinMax.MinUnion.field_ldouble)
                    MinMax.MinUnion.field_ldouble = *(long double *)Element;
                if (*(long double *)Element > MinMax.MaxUnion.field_ldouble)
                    MinMax.MaxUnion.field_ldouble = *(long double *)Element;
                break;
            case adios2::DataType::FloatComplex:
            case adios2::DataType::DoubleComplex:
            case adios2::DataType::String:
            case adios2::DataType::Struct:
                break;
        }
    }

void HermesEngine::LoadMetadata() {
    std::string filename = "step_" +  std::to_string(currentStep) +
            "_rank_" +  std::to_string(rank);
    hapi::Bucket bkt_vars = HERMES->GetBucket(filename);
    hapi::Context ctx_vars;
    std::vector<hermes::BlobId> blobIds = bkt_vars.GetContainedBlobIds();
    for (const auto &blobId : blobIds) {
        hermes::Blob blob;
        bkt_vars.Get(blobId, blob, ctx_vars);
        VariableMetadata variableMetadata =
                MetadataSerializer::DeserializeMetadata(blob);
        std::string varName = bkt_vars.GetBlobName(blobId);
        std::cout << "Variable is: " << varName << std::endl;
        listOfVars.push_back(varName);
        DefineVariable(variableMetadata);
    }
}

void HermesEngine::DefineVariable(VariableMetadata variableMetadata) {

    if(currentStep != 1) {
        // If the metadata is defined delete current value to update it
        m_IO.RemoveVariable(variableMetadata.name);
    }
#define DEFINE_VARIABLE(T) \
    if (adios2::helper::GetDataType<T>() == variableMetadata.getDataType()) {     \
         adios2::core::Variable<T> *variable = &(m_IO.DefineVariable<T>( \
                      variableMetadata.name, \
                      variableMetadata.shape, \
                      variableMetadata.start, \
                      variableMetadata.count, \
                      variableMetadata.constantShape));                           \
         variable->m_AvailableStepsCount = 1;                                   \
         variable->m_ShapeID = adios2::ShapeID::GlobalArray;                            \
         variable->m_SingleValue = false;                                       \
         variable->m_Min = std::numeric_limits<T>::max();                       \
         variable->m_Max = std::numeric_limits<T>::min();                         \
         variable->m_Engine = this;                                               \
          }
        ADIOS2_FOREACH_STDTYPE_1ARG(DEFINE_VARIABLE)
#undef DEFINE_VARIABLE
}

adios2::StepStatus HermesEngine::BeginStep(adios2::StepMode mode,
                                           const float timeoutSeconds) {
    std::cout << __func__ << std::endl;

  // Increase currentStep and save it in Hermes
  IncrementCurrentStep();

  if (m_OpenMode == adios2::Mode::Read) {
    // Retrieve the metadata
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
void HermesEngine::HermesPut(const std::string &bucket_name,
                             const std::string &blob_name, size_t blob_size, T values) {
  hapi::Bucket bkt = HERMES->GetBucket(bucket_name);
  hapi::Context ctx;
  hermes::Blob blob(blob_size);
  hermes::BlobId blob_id;
  memcpy(blob.data(), values, blob_size);
  bkt.Put(blob_name, blob, blob_id, ctx);
}

hermes::Blob HermesEngine::HermesGet(const std::string &bucket_name, const std::string &blob_name) {
  hapi::Bucket bkt = HERMES->GetBucket(bucket_name);
  hapi::Context ctx;
  hermes::Blob blob;
  hermes::BlobId blob_id;
  bkt.GetBlobId(blob_name, blob_id);
  bkt.Get(blob_id, blob, ctx);
  return blob;
}


    template<typename T>
void HermesEngine::DoGetDeferred_(
    const adios2::core::Variable<T> &variable, T *values) {
  std::cout << __func__ << std::endl;

  // Retrieve the value of the variable in the current step
  std::string filename = variable.m_Name + "_step_" +
      std::to_string(currentStep) + "_rank" + std::to_string(rank);

  hermes::Blob blob = HermesGet(filename,variable.m_Name);
  memcpy(values, blob.data(), blob.size());
}

template<typename T>
void HermesEngine::DoPutDeferred_(
    const adios2::core::Variable<T> &variable, const T *values) {
  std::cout << __func__ << " - Step: " << currentStep
            << ", Rank: " << rank << ", Putting: " << variable.m_Name
            << " with value: " << *values << std::endl;

  // Create a bucket with the associated step and process rank
  std::string filename = variable.m_Name + "_step_" +
      std::to_string(currentStep) + "_rank" + std::to_string(rank);

  std::cout << "Bucket name is: " << filename << std::endl;

  HermesPut(filename, variable.m_Name ,
            variable.SelectionSize() * sizeof(T), values);

  // Check if the value is already in the list
  auto it = std::find(listOfVars.begin(),
                      listOfVars.end(),
                      variable.m_Name);

  // Update the metadata bucket in hermes with the new variable
  if (it == listOfVars.end() && filename.compare(0, 4, "step") != 0) {

      std::string filename_metadata = "step_" +  std::to_string(currentStep) +
                             "_rank_" +  std::to_string(rank);
      listOfVars.push_back(variable.m_Name);

      std::string serializedMetadata =
              MetadataSerializer::SerializeMetadata<T>(variable);

      HermesPut(filename_metadata, variable.m_Name
              ,serializedMetadata.size(), serializedMetadata.data());

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

