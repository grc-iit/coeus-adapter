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
/**
 * Construct the HermesEngine.
 *
 * This is a wrapper around Init_, which performs the main custom
 * initialization of the engine. The PluginEngineInterface will store
 * the "io" variable in the "m_IO" variable.
 * */
HermesEngine::HermesEngine(adios2::core::IO &io, // NOLINT
                           const std::string &name, const adios2::Mode mode,
                           adios2::helper::Comm comm)
    : adios2::plugin::PluginEngineInterface(io, name, mode, comm.Duplicate()) {
  Hermes = std::make_shared<coeus::Hermes>();
  Init_();
//  m_Comm.Barrier();
}

/**
 * Test initializer
 * */
HermesEngine::HermesEngine(std::shared_ptr<coeus::IHermes> h,
                           std::shared_ptr<coeus::MPI> mpi,
                           adios2::core::IO &io, const std::string &name,
                           const adios2::Mode mode, adios2::helper::Comm comm)
    : adios2::plugin::PluginEngineInterface(io, name, mode, comm.Duplicate()) {
  Hermes = h;
  Init_();
}

/**
 * Initialize the engine.
 * */
void HermesEngine::Init_() {
  // Logger setup
  // Console log
  auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
  console_sink->set_level(spdlog::level::info);
  console_sink->set_pattern("%^[Coeus engine] [%!:%# @ %s] [%l] %$ %v");

  // File log
  auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
      "logs/engine_test.txt", true);
  file_sink->set_level(spdlog::level::trace);
  file_sink->set_pattern("%^[Coeus engine] [%!:%# @ %s] [%l] %$ %v");

  // Merge Log
  spdlog::logger logger("debug_logger", {console_sink, file_sink});
  logger.set_level(spdlog::level::debug);
  engine_logger = std::make_shared<spdlog::logger>(logger);

//  engine_logger->info("rank {}", rank);

  // MPI setup
  rank = m_Comm.Rank();
  comm_size = m_Comm.Size();

  // Identifier, should be the file, but we dont get it
  uid = this->m_IO.m_Name;

  // Configuration Setup through the Adios xml configuration
  auto params = m_IO.m_Parameters;
  if (params.find("OPFile") != params.end()) {
    std::string opFile = params["OPFile"];
    if (rank == 0)
      std::cout << "OPFile: " << opFile << std::endl;
    try {
      operationMap = YAMLParser(opFile).parse();
    } catch (std::exception &e) {
      engine_logger->warn("Could not parse operation file", rank);
      throw e;
    }
  }
  if (params.find("ppn") != params.end()) {
    ppn = stoi(params["ppn"]);
    if (rank == 0)
      std::cout << "PPN: " << ppn << std::endl;
  }

    if (params.find("limit") != params.end()) {
        limit = stoi(params["limit"]);
        if (rank == 0)
            std::cout << "limit: " << limit << std::endl;
    }

  if (params.find("lookahead") != params.end()) {
    lookahead = stoi(params["lookahead"]);
    if (rank == 0)
      std::cout << "lookahead: " << lookahead << std::endl;
  }
  else{
    lookahead = 2;
  }

  if (params.find("VarFile") != params.end()) {
    std::string varFile = params["VarFile"];
    if (rank == 0)
      std::cout << "varFile: " << varFile << std::endl;
    try {
      variableMap = YAMLParser(varFile).parse();
    } catch (std::exception &e) {
      engine_logger->warn("Could not parse variable file", rank);
      throw e;
    }
  }
  // Hermes setup
  if (!Hermes->connect()) {
    engine_logger->warn("Could not connect to Hermes", rank);
    throw coeus::common::ErrorException(HERMES_CONNECT_FAILED);
  }
  if (rank == 0)
    std::cout << "Connected to Hermes" << std::endl;

  if (params.find("db_file") != params.end()) {
    db_file = params["db_file"];
    db = new SQLiteWrapper(db_file);
    if (rank % ppn == 0) {
      db->createTables();
      std::cout << "DB_FILE: " << db_file << std::endl;
    }
    TRANSPARENT_HERMES();
    client.CreateRoot(DomainId::GetGlobal(), "db_operation", db_file);
    if (rank == 0)
      std::cout << "Done with root" << std::endl;
  } else {
    throw std::invalid_argument("db_file not found in parameters");
  }
  open = true;
}

/**
 * Close the Engine.
 * */
void HermesEngine::DoClose(const int transportIndex) {
  open = false;
}

HermesEngine::~HermesEngine() {
  delete db;
}

/**
 * Handle step operations.
 * */

bool HermesEngine::Promote(int step){
    bool success = true;
    if(step < total_steps) {
        auto var_locations = db->getAllBlobs(currentStep + lookahead, rank);
        for (const auto &location : var_locations) {
            success &= Hermes->Prefetch(location.bucket_name, location.blob_name);
        }
    }
    return success;
}

bool HermesEngine::Demote(int step){
    bool success = true;
    if (step > 0) {
        auto var_locations = db->getAllBlobs(step, rank);
        for (const auto &location: var_locations) {
            success &= Hermes->Demote(location.bucket_name, location.blob_name);
        }
    }
    return success;
}

adios2::StepStatus HermesEngine::BeginStep(adios2::StepMode mode,
                                           const float timeoutSeconds) {
  IncrementCurrentStep();
  if (m_OpenMode == adios2::Mode::Read) {
    if (total_steps == -1)
      total_steps = db->GetTotalSteps(uid);

    if (currentStep > total_steps) {
      return adios2::StepStatus::EndOfStream;
    }
    LoadMetadata();
  }
  std::string bucket_name =
      "step_" + std::to_string(currentStep) + "_rank" + std::to_string(rank);
  Hermes->GetBucket(bucket_name);

  if(m_OpenMode == adios2::Mode::Read){
      for(int i = 0; i < num_layers; i++) {
          Promote(currentStep + lookahead + i);
      }
  }
  if(m_OpenMode == adios2::Mode::Write){
      for(int i = 0; i < num_layers; i++) {
          Demote(currentStep - lookahead - i);
      }
  }

  return adios2::StepStatus::OK;
}

void HermesEngine::IncrementCurrentStep() { currentStep++; }

size_t HermesEngine::CurrentStep() const { return currentStep; }

void HermesEngine::ComputeDerivedVariables() {
  auto const &m_VariablesDerived = m_IO.GetDerivedVariables();
  auto const &m_Variables = m_IO.GetVariables();
  // parse all derived variables
  if(rank == 0) {
      std::cout << " Parsing " << m_VariablesDerived.size() << " derived variables"
                << std::endl;
  }
  for (auto it = m_VariablesDerived.begin(); it != m_VariablesDerived.end();
       it++) {
    // identify the variables used in the derived variable
    auto derivedVar =
        dynamic_cast<adios2::core::VariableDerived *>((*it).second.get());
    std::vector<std::string> varList = derivedVar->VariableNameList();
    // to create a mapping between variable name and the varInfo (dim and data
    // pointer)
    std::map<std::string, adios2::MinVarInfo> nameToVarInfo;
    for (auto varName : varList) {
      auto itVariable = m_Variables.find(varName);
          if (itVariable == m_Variables.end())
            std::cout <<"throw error commented" <<std::endl;
//              adios2::helper::Throw<std::invalid_argument>("Core", "IO",
//              "DefineDerivedVariable",
//                                                     "using undefine
//                                                     variable " +
//                                                     varName +
//                                                         " in defining
//                                                         the derived
//                                                         variable " +
//                                                         (*it).second->m_Name);
      // extract the dimensions and data for each variable
      adios2::core::VariableBase *varBase = itVariable->second.get();
      auto blob = Hermes->bkt->Get(varName);

      adios2::MinBlockInfo blk({0, 0, itVariable->second.get()->m_Start.data(),
                                itVariable->second.get()->m_Count.data(),
                                adios2::MinMaxStruct(), blob.data()});

      // if this is the first block for the variable
      auto entry = nameToVarInfo.find(varName);
      if (entry == nameToVarInfo.end()) {
        // create an mvi structure and add the new block to it
        int varDim = itVariable->second.get()->m_Shape.size();
        adios2::MinVarInfo mvi(varDim,
                               itVariable->second.get()->m_Shape.data());
        mvi.BlocksInfo.push_back(blk);
        nameToVarInfo.insert({varName, mvi});
      } else {
        // otherwise add the current block to the existing mvi
        entry->second.BlocksInfo.push_back(blk);
      }
    }

    // compute the values for the derived variables that are not type
    // ExpressionString
    std::vector<std::tuple<void *, adios2::Dims, adios2::Dims>>
        DerivedBlockData;
    if (derivedVar->GetDerivedType() !=
        adios2::DerivedVarType::ExpressionString) {
      DerivedBlockData = derivedVar->ApplyExpression(nameToVarInfo);
    }

    for (auto derivedBlock : DerivedBlockData) {
#define DEFINE_VARIABLE_PUT(T)       \
  if (adios2::helper::GetDataType<T>() == derivedVar->m_Type) { \
    T* data = static_cast<T *>(std::get<0>(derivedBlock));\
    PutDerived(*derivedVar, data);    \
  }
  ADIOS2_FOREACH_ATTRIBUTE_PRIMITIVE_STDTYPE_1ARG(DEFINE_VARIABLE_PUT)
#undef DEFINE_VARIABLE_PUT
      free(std::get<0>(derivedBlock));
    }
  }
}

void HermesEngine::EndStep() {
  ComputeDerivedVariables();
  if (m_OpenMode == adios2::Mode::Write) {
    if (rank % ppn == 0) {
      DbOperation db_op(uid, currentStep);
      client.Mdm_insertRoot(DomainId::GetLocal(), db_op);
    }
  }
  delete Hermes->bkt;
}

/**
 * Metadata operations.
 * */
bool HermesEngine::VariableMinMax(const adios2::core::VariableBase &Var,
                                  const size_t Step,
                                  adios2::MinMaxStruct &MinMax) {
  // We initialize the min and max values
  MinMax.Init(Var.m_Type);

  // Obtain the blob from Hermes using the filename and variable name
  hermes::Blob blob = Hermes->bkt->Get(Var.m_Name);

#define DEFINE_VARIABLE(T)                                                     \
  if (adios2::helper::GetDataType<T>() == Var.m_Type) {                        \
    size_t dataSize = blob.size() / sizeof(T);                                 \
    const T *data = reinterpret_cast<const T *>(blob.data());                  \
    for (size_t i = 0; i < dataSize; ++i) {                                    \
      void *elementPtr =                                                       \
          const_cast<void *>(static_cast<const void *>(&data[i]));             \
      ApplyElementMinMax(MinMax, Var.m_Type, elementPtr);                      \
    }                                                                          \
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

template <typename T>
T *HermesEngine::SelectUnion(adios2::PrimitiveStdtypeUnion &u) {
  return reinterpret_cast<T *>(&u);
}

template <typename T>
void HermesEngine::ElementMinMax(adios2::MinMaxStruct &MinMax, void *element) {
  T *min = SelectUnion<T>(MinMax.MinUnion);
  T *max = SelectUnion<T>(MinMax.MaxUnion);
  T *value = static_cast<T *>(element);
  if (*value < *min) {
    min = value;
  }
  if (*value > *max) {
    max = value;
  }
}

void HermesEngine::LoadMetadata() {
  auto metadata_vector = db->GetAllVariableMetadata(currentStep, rank);
  for (auto &variableMetadata : metadata_vector) {

    DefineVariable(variableMetadata);
  }
}

void HermesEngine::DefineVariable(const VariableMetadata &variableMetadata) {
  if (currentStep != 1) {
    // If the metadata is defined delete current value to update it
    m_IO.RemoveVariable(variableMetadata.name);
  }

#define DEFINE_VARIABLE(T)                                                     \
  if (adios2::helper::GetDataType<T>() ==                                      \
      adios2::helper::GetDataTypeFromString(variableMetadata.dataType)) {      \
    adios2::core::Variable<T> *variable = &(m_IO.DefineVariable<T>(            \
        variableMetadata.name, variableMetadata.shape, variableMetadata.start, \
        variableMetadata.count, variableMetadata.constantShape));              \
    variable->m_AvailableStepsCount = 1;                                       \
    variable->m_SingleValue = false;                                           \
    variable->m_Min = std::numeric_limits<T>::max();                           \
    variable->m_Max = std::numeric_limits<T>::min();                           \
    variable->m_Engine = this;                                                 \
  }
  ADIOS2_FOREACH_STDTYPE_1ARG(DEFINE_VARIABLE)
#undef DEFINE_VARIABLE
}

template <typename T>
void HermesEngine::DoGetDeferred_(const adios2::core::Variable<T> &variable,
                                  T *values) {
  auto blob = Hermes->bkt->Get(variable.m_Name);
  memcpy(values, blob.data(), blob.size());
}

template <typename T>
void HermesEngine::DoPutDeferred_(const adios2::core::Variable<T> &variable,
                                  const T *values) {
  Hermes->bkt->Put(variable.m_Name, variable.SelectionSize() * sizeof(T), values);

  DbOperation db_op = generateMetadata(variable);
  client.Mdm_insertRoot(DomainId::GetLocal(), db_op);
}

template<typename T>
DbOperation HermesEngine::generateMetadata(adios2::core::Variable<T> variable) {
  VariableMetadata vm(variable.m_Name, variable.m_Shape, variable.m_Start,
                      variable.m_Count, variable.IsConstantDims(), true,
                      adios2::ToString(variable.m_Type));
  BlobInfo blobInfo(Hermes->bkt->name, variable.m_Name);
  return DbOperation(currentStep, rank, std::move(vm), variable.m_Name, std::move(blobInfo));
}

DbOperation HermesEngine::generateMetadata(adios2::core::VariableDerived variable, float *values, int total_count) {
  VariableMetadata vm(variable.m_Name, variable.m_Shape, variable.m_Start,
                     variable.m_Count, variable.IsConstantDims(), true,
                     adios2::ToString(variable.m_Type));
  BlobInfo blobInfo(Hermes->bkt->name, variable.m_Name);

//  if(total_count < 1) {
//    return DbOperation(currentStep, rank, std::move(vm), variable.m_Name, std::move(blobInfo));
//  }
//
//  float min = std::numeric_limits<float>::max();
//  float max = std::numeric_limits<float>::lowest();
//
//  for (size_t i = 0; i < total_count; i++) {
//    // Calculate the address of the current element
//    char* elementPtr = reinterpret_cast<char*>(values) + (i * variable.m_ElementSize);
//    // Cast the element to the correct type
//    float element = *reinterpret_cast<float*>(elementPtr);
//
//    // Update min and max
//    if (element < min) min = element;
//    if (element > max) max = element;
//  }
//  if (min == std::numeric_limits<float>::max() || max == std::numeric_limits<float>::lowest()) {
//    std::cout << "BUUUUUG : Bucekt " << Hermes->bkt->name << " blob " << variable.m_Name << " min " << min << " max " << max
//              << " total count " << total_count << std::endl;
//  }
//  derivedSemantics derived_semantics(min, max);
//
//  std::cout << "step_" << currentStep << "_rank" << rank <<  variable.m_Name << " derived min " << min << " max " << max << std::endl;
//  return DbOperation(currentStep, rank, std::move(vm), variable.m_Name, std::move(blobInfo), derived_semantics);
    return DbOperation(currentStep, rank, std::move(vm), variable.m_Name, std::move(blobInfo));

}

template <typename T>
void HermesEngine::PutDerived(adios2::core::VariableDerived variable,
                              T *values) {
  std::string name = variable.m_Name;
  int total_count = 1;
  for (auto count : variable.m_Count) {
    total_count *= count;
  }

  Hermes->bkt->Put(name, total_count * sizeof(T), values);

  DbOperation db_op = generateMetadata(variable, (float*) values, total_count);
  client.Mdm_insertRoot(DomainId::GetLocal(), db_op);
}

} // namespace coeus
/**
 * This is how ADIOS figures out where to dynamically load the engine.
 * */
extern "C" {

/** C wrapper to create engine */
coeus::HermesEngine *EngineCreate(adios2::core::IO &io, // NOLINT
                                  const std::string &name,
                                  const adios2::Mode mode,
                                  adios2::helper::Comm comm) {
  (void)mode;
  return new coeus::HermesEngine(io, name, mode, comm.Duplicate());
}

/** C wrapper to destroy engine */
void EngineDestroy(coeus::HermesEngine *obj) { delete obj; }
}
