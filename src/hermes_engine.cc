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

namespace coeus {
/*
 *
 * class MyProject(){
 * public:
 *  hermes = HERMES;
 *
 *  MyProject() = default;
 *
 *  * class MyProject(){
 * public:
 *
 *  MyProject(hermes h_object)hermes(h_object);
 *
 */


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
//  if (comm.Rank() == 0) std::cout << "NAMING: " << name << " " << this->m_Name << " "
//  << m_Name << " " << this->m_IO.m_Name << std::endl;
  Hermes = std::make_shared<coeus::Hermes>();
  mpiComm = std::make_shared<coeus::MPI>(comm.Duplicate());
  Init_();
  engine_logger->info("rank {} with name {} and mode {}", rank, name, adios2::ToString(mode));
}

/**
 * Test initializer
 * */
HermesEngine::HermesEngine(std::shared_ptr<coeus::IHermes> h,
                           std::shared_ptr<coeus::MPI> mpi,
                           adios2::core::IO &io,
                           const std::string &name,
                           const adios2::Mode mode,
                           adios2::helper::Comm comm)
    : adios2::plugin::PluginEngineInterface(io, name, mode, comm.Duplicate()) {
  Hermes = h;
  mpiComm = mpi;
  Init_();
  engine_logger->info("rank {} with name {} and mode {}", rank, name, adios2::ToString(mode));
}

/**
* Initialize the engine.
* */
void HermesEngine::Init_() {
  // Logger setup
  // Console log
  auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
  console_sink->set_level(spdlog::level::warn);
  console_sink->set_pattern("%^[Coeus engine] [%!:%# @ %s] [%l] %$ %v");

  //File log
  auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
      "logs/engine_test.txt", true);
  file_sink->set_level(spdlog::level::trace);
  file_sink->set_pattern("%^[Coeus engine] [%!:%# @ %s] [%l] %$ %v");

  //Merge Log
  spdlog::logger logger("debug_logger", {console_sink, file_sink});
  logger.set_level(spdlog::level::debug);
  engine_logger = std::make_shared<spdlog::logger>(logger);

  engine_logger->info("rank {}", rank);

  //MPI setup
  rank = mpiComm->getGlobalRank();
  comm_size = mpiComm->getGlobalSize();

  //Identifier, should be the file, but we dont get it
  uid = this->m_IO.m_Name;

  //Configuration Setup through the Adios xml configuration
  auto params = m_IO.m_Parameters;
  if(params.find("OPFile") != params.end()){
    std::string opFile = params["OPFile"];
    if(rank==0)std::cout<< "OPFile: " << opFile << std::endl;
    try{
      operationMap = YAMLParser(opFile).parse();
    }
    catch (std::exception &e){
      engine_logger->warn("Could not parse operation file", rank);
      throw e;
    }
  }
  if(params.find("VarFile") != params.end()){
    std::string varFile = params["VarFile"];
    if(rank==0)std::cout<< "varFile: " << varFile << std::endl;
    try{
      variableMap = YAMLParser(varFile).parse();
    }
    catch (std::exception &e){
      engine_logger->warn("Could not parse variable file", rank);
      throw e;
    }
  }

  if(params.find("db_file") != params.end()){
    std::string db_file = params["db_file"];
  } else{
    throw std::invalid_argument("db_file not found in parameters");
  }

  //Hermes setup
  if(!Hermes->connect()){
    engine_logger->warn("Could not connect to Hermes", rank);
    throw coeus::common::ErrorException(HERMES_CONNECT_FAILED);
  }
  open = true;
}

/**
 * Close the Engine.
 * */
void HermesEngine::DoClose(const int transportIndex) {
  std::cout << "Close Inside Enter" << std::endl;
  engine_logger->info("rank {}", rank);
  open = false;
  mpiComm->free();
  std::cout << "Close Inside Finsih" << std::endl;
}

HermesEngine::~HermesEngine() {
  std::cout << "Close des Enter" << std::endl;
  engine_logger->info("rank {}", rank);
  std::cout << "Close des Finsih" << std::endl;
}

/**
 * Handle step operations.
 * */

adios2::StepStatus HermesEngine::BeginStep(adios2::StepMode mode,
                                           const float timeoutSeconds) {
  engine_logger->info("rank {}", rank);
  IncrementCurrentStep();
  if (m_OpenMode == adios2::Mode::Read) {
    auto bkt = Hermes->GetBucket("total_steps");
    hermes::Blob blob = bkt->Get("total_steps_" + uid);
    total_steps = *reinterpret_cast<const int *>(blob.data());

    if (currentStep > total_steps) {
      return adios2::StepStatus::EndOfStream;
    }
    LoadMetadata();
  }
  return adios2::StepStatus::OK;
}

void HermesEngine::IncrementCurrentStep() {
  engine_logger->info("rank {}", rank);
  currentStep++;
}

size_t HermesEngine::CurrentStep() const {
  engine_logger->info("rank {}", rank);
  return currentStep;
}

void HermesEngine::EndStep() {
  engine_logger->info("rank {}", rank);
  if (m_OpenMode == adios2::Mode::Write) {
    if(rank == 0) {
      auto blob_name = "total_steps_" + uid;
      auto bkt = Hermes->GetBucket("total_steps");
      bkt->Put(blob_name, sizeof(int), &currentStep);
    }
  }
}

/**
 * Metadata operations.
 * */
bool HermesEngine::VariableMinMax(const adios2::core::VariableBase &Var,
                                  const size_t Step,
                                  adios2::MinMaxStruct &MinMax) {
  // We initialize the min and max values
  MinMax.Init(Var.m_Type);

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

void HermesEngine::ApplyElementMinMax(adios2::MinMaxStruct &MinMax,
                                      adios2::DataType Type, void *Element) {

  switch (Type) {
    case adios2::DataType::Int8:
      ElementMinMax<int8_t>(MinMax, Element);
      break;
    case adios2::DataType::Int16:
      ElementMinMax<int16_t>(MinMax, Element);
      break;
    case adios2::DataType::Int32:
      ElementMinMax<int32_t>(MinMax, Element);
      break;
    case adios2::DataType::Int64:
      ElementMinMax<int64_t>(MinMax, Element);
      break;
    case adios2::DataType::UInt8:
      ElementMinMax<uint8_t>(MinMax, Element);
      break;
    case adios2::DataType::UInt16:
      ElementMinMax<uint16_t>(MinMax, Element);
      break;
    case adios2::DataType::UInt32:
      ElementMinMax<uint32_t>(MinMax, Element);
      break;
    case adios2::DataType::UInt64:
      ElementMinMax<uint64_t>(MinMax, Element);
      break;
    case adios2::DataType::Float:
      ElementMinMax<float>(MinMax, Element);
      break;
    case adios2::DataType::Double:
      ElementMinMax<double>(MinMax, Element);
      break;
    case adios2::DataType::LongDouble:
      ElementMinMax<long double>(MinMax, Element);
      break;
    default:
      /*
       *     case adios2::DataType::None
       *     adios2::DataType::Char
       *     case adios2::DataType::FloatComplex
       *     case adios2::DataType::DoubleComplex
       *     case adios2::DataType::String
       *     case adios2::DataType::Struct
       */
      break;
  }
}

template<typename T>
T* HermesEngine::SelectUnion(adios2::PrimitiveStdtypeUnion &u) {
  return reinterpret_cast<T*>(&u);
}

template<typename T>
void HermesEngine::ElementMinMax(adios2::MinMaxStruct &MinMax, void* element) {
  T* min = SelectUnion<T>(MinMax.MinUnion);
  T* max = SelectUnion<T>(MinMax.MaxUnion);
  T* value = static_cast<T*>(element);
  if (*value < *min) {
    min = value;
  }
  if (*value > *max) {
      max = value;
  }
}

void HermesEngine::LoadMetadata() {
  std::string filename = "step_" + std::to_string(currentStep) +
      "_rank_" + std::to_string(rank);
  std::cout << "Load Metadata: " << filename << std::endl;
  auto bkt = Hermes->GetBucket(filename);
  std::cout << "Get Bucket Metadata: " << filename << std::endl;
  std::vector<hermes::BlobId> blobIds = bkt->GetContainedBlobIds();
  std::cout << "blobIds.size(): " << blobIds.size() << std::endl;
  for (const auto &blobId : blobIds) {
    std::cout << "blobId: " << blobId << std::endl;
    hermes::Blob blob = bkt->Get(blobId);
    VariableMetadata variableMetadata =
        MetadataSerializer::DeserializeMetadata(blob);
    listOfVars.push_back(bkt->GetBlobName(blobId));
    std::cout << "variableMetadata: " << variableMetadata << std::endl;
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
  std::cout << "Done with Get" << std::endl;
}

template<typename T>
void HermesEngine::DoPutDeferred_(
    const adios2::core::Variable<T> &variable, const T *values) {
  std::cout << __func__ << std::endl;

  // Create a bucket with the associated step and process rank
  std::string bucket_name = variable.m_Name + "_step_" +
      std::to_string(currentStep) + "_rank" + std::to_string(rank);

  auto bkt = Hermes->GetBucket(bucket_name);
  bkt->Put(variable.m_Name, variable.SelectionSize() * sizeof(T), values);

  std::string bucket_name_metadata = "step_" + std::to_string(currentStep) +
      "_rank_" + std::to_string(rank);

  std::string serializedMetadata =
      MetadataSerializer::SerializeMetadata<T>(variable);

  auto bkt_metadata = Hermes->GetBucket(bucket_name_metadata);
  auto status = bkt_metadata->Put(variable.m_Name, serializedMetadata.size(), serializedMetadata.data());
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

