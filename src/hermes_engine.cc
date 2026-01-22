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
#include "comms/CTEHermes.h"
#include <chimaera/chimaera.h>
#include <chimaera/admin/admin_client.h>
#include <chimaera/module_manager.h>
#include <wrp_cte/core/core_client.h>
#include <wrp_cte/core/core_tasks.h>
#include <chrono>

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
  // Create CTEHermes instance - it will handle CTE initialization via connect()
  hermes_ = new coeus::CTEHermes();
  
  // Initialize CTE via CTEHermes::connect()
  if (!hermes_->connect()) {
    delete hermes_;
    hermes_ = nullptr;
    throw std::runtime_error("Failed to initialize CTE via CTEHermes::connect()");
  }
  
  //  mpiComm = std::make_shared<coeus::MPI>(comm.Duplicate());
  Init_();
  //engine_logger->info("rank {} with name {} and mode {}", rank, name, adios2::ToString(mode));


}

/**
 * Test initializer
 * */
HermesEngine::HermesEngine(std::shared_ptr<coeus::MPI> mpi,
                           adios2::core::IO &io, const std::string &name,
                           const adios2::Mode mode, adios2::helper::Comm comm)
    : adios2::plugin::PluginEngineInterface(io, name, mode, comm.Duplicate()) {
  // For testing: create CTEHermes instance
  // CTEHermes::connect() will check if CTE is already initialized
  hermes_ = new coeus::CTEHermes();
  
  // Initialize CTE via CTEHermes::connect() if not already initialized
  if (!hermes_->connect()) {
    delete hermes_;
    hermes_ = nullptr;
    throw std::runtime_error("Failed to initialize CTE via CTEHermes::connect()");
  }
  
  Init_();
  //engine_logger->info("rank {} with name {} and mode {}", rank, name, adios2::ToString(mode));

}

/**
 * Initialize the engine.
 * */
void HermesEngine::Init_() {
  // Initialize rank to 0 (will be set by rank consensus)
  rank = 0;
  comm_size = 0;  // Initialize comm_size as well

  // initiate the trace manager
   std::random_device rd;  // Obtain a random seed
    std::mt19937 gen(rd()); // Mersenne Twister generator
    std::uniform_int_distribution<> dis(1, 10000);
    // Generate a random number
    int randomNumber = dis(gen);
    // Step 2: Convert the random number to a string
    std::string randomNumberStr = std::to_string(randomNumber);

    // Step 3: Add the random number to a base string
    std::string baseString = "logs/engine_test_";
    std::string logname = baseString + randomNumberStr + ".txt";

  auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
  console_sink->set_level(spdlog::level::trace);
  console_sink->set_pattern("%^[Coeus engine] [%!:%# @ %s] [%l] %$ %v");

  // File log
  auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
      logname, true);
  file_sink->set_level(spdlog::level::trace);
  file_sink->set_pattern("%^[Coeus engine] [%!:%# @ %s] [%l] %$ %v");
 
  
  // File log for metadata collection
  #ifdef Meta_enabled
  auto file_sink2 = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
      "logs/metadataCollect_get.txt", true);
  file_sink2->set_level(spdlog::level::trace);
  file_sink2->set_pattern("%v");
  spdlog::logger logger2("metadata_logger_get", {file_sink2});
  logger2.set_level(spdlog::level::trace);
  meta_logger_get = std::make_shared<spdlog::logger>(logger2);
  meta_logger_get->info(
      "\nName, shape, start, Count, Constant Shape, Time, selectionSize, sizeofVariable\n ShapeID, steps, stepstart, blockID, blob_name, tag_name, processor, process");

  auto file_sink3 = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
      "logs/metadataCollect_put.txt", true);
  file_sink3->set_level(spdlog::level::trace);
  file_sink3->set_pattern("%v");
  spdlog::logger logger3("metadata_logger_put", {file_sink3});
  logger3.set_level(spdlog::level::trace);
  meta_logger_put = std::make_shared<spdlog::logger>(logger3);
  meta_logger_put->info(
      "\nName, shape, start, Count, Constant Shape, Time, selectionSize, sizeofVariable, \nShapeID, steps, stepstart, blockID, blob_name, tag_name, processor, process");
#endif

  //Merge Log

  spdlog::logger logger("debug_logger", {console_sink, file_sink});
  logger.set_level(spdlog::level::debug);
  engine_logger = std::make_shared<spdlog::logger>(logger);

  // Initialize Chimaera (Context-Runtime) for task management
  if (!chi::CHIMAERA_INIT(chi::ChimaeraMode::kClient, true)) {
    std::cout << "Could not initialize Chimaera" << std::endl;
    throw coeus::common::ErrorException(HERMES_CONNECT_FAILED);
  }
  std::cout << "Initialized Chimaera" << std::endl;

  // Load required ChiMod modules (rankConsensus and coeus_mdm)
  if (CHI_MODULE_MANAGER) {
    // Initialize ModuleManager if not already initialized
    if (!CHI_MODULE_MANAGER->IsInitialized()) {
      if (!CHI_MODULE_MANAGER->ServerInit()) {
        engine_logger->warn("ModuleManager auto-discovery failed, modules may not be loaded");
      } else {
        engine_logger->info("ModuleManager initialized, modules auto-discovered");
      }
    }
    
    // Verify required modules are loaded
    auto* rankConsensus_mod = CHI_MODULE_MANAGER->GetChiMod("chimaera_rankConsensus");
    auto* coeus_mdm_mod = CHI_MODULE_MANAGER->GetChiMod("chimaera_coeus_mdm");
    
    if (!rankConsensus_mod) {
      engine_logger->warn("rankConsensus module not found - may fail during pool creation");
    } else {
      engine_logger->info("rankConsensus module loaded: {}", rankConsensus_mod->lib_path);
    }
    
    if (!coeus_mdm_mod) {
      engine_logger->warn("coeus_mdm module not found - may fail during pool creation");
    } else {
      engine_logger->info("coeus_mdm module loaded: {}", coeus_mdm_mod->lib_path);
    }
  } else {
    engine_logger->warn("CHI_MODULE_MANAGER not available - modules may not be loaded");
  }

  // Create admin client (required for pool management)
  chimaera::admin::Client admin_client(chi::kAdminPoolId);
  auto admin_create_task = admin_client.AsyncCreate(chi::PoolQuery::Local(), "admin", chi::kAdminPoolId);
  admin_create_task.Wait();
  if (admin_create_task->GetReturnCode() != 0) {
    engine_logger->error("Failed to create admin container");
    throw coeus::common::ErrorException(HERMES_CONNECT_FAILED);
  }
  admin_client.Init(admin_create_task->new_pool_id_);

  // Initialize rank consensus pool first to get rank
  // Note: rank is initialized to 0 by default, but we'll get the actual rank from consensus
  rankConsensus_pool_id_ = chi::PoolId(8001, 0);
  rank_consensus = chimaera::rankConsensus::Client(rankConsensus_pool_id_);
  rank_consensus.Create(chi::PoolQuery::Local(), "rankConsensus", rankConsensus_pool_id_);
  rank = rank_consensus.GetRank(chi::PoolQuery::Local());
  
  std::cout << "Rank consensus initialized, assigned rank: " << rank << std::endl;
 
  //Identifier, should be the file, but we don't get it
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
  // find the ppn
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

  // Chimaera setup for metadata management

  if (params.find("db_file") != params.end()) {
    db_file = params["db_file"];
    db = new SQLiteWrapper(db_file);
    
    // Create coeus_mdm pool
    coeus_mdm_pool_id_ = chi::PoolId(8000, 0);
    client = chimaera::coeus_mdm::Client(coeus_mdm_pool_id_);
    client.Create(chi::PoolQuery::Local(), "db_operation", coeus_mdm_pool_id_, db_file);
    
    if (rank % ppn == 0) {
      db->createTables();
    }
  } else {
    throw std::invalid_argument("db_file not found in parameters");
  }
  if(params.find("execution_order") != params.end()) {
      adiosOutput = params["execution_order"];
  }
  open = true;

}

/**
 * Close the Engine.
 * */
void HermesEngine::DoClose(const int transportIndex) {
  TRACE_FUNC("engine close");
  open = false;
}

HermesEngine::~HermesEngine() {
  TRACE_FUNC();
  delete db;
  if (hermes_) {
    delete hermes_;
    hermes_ = nullptr;
  }
}

/**
 * Handle step operations.
 * */

bool HermesEngine::Promote(int step){
    // CTE handles data placement automatically based on access patterns
    // Prefetch is handled by CTE's data placement engine
    // bool success = true;
    // if(step < total_steps) {
    //     auto var_locations = db->getAllBlobs(currentStep + lookahead, rank);
    //     for (const auto &location : var_locations) {
    //         success &= Hermes->Prefetch(location.tag_name, location.blob_name);
    //     }
    // }
    // return success;
    // This is a no-op - CTE will automatically promote based on scoring
    (void)step;  // Suppress unused parameter warning
    return true;
}

bool HermesEngine::Demote(int step){
    // CTE handles data placement automatically based on access patterns
    // Demote is handled by CTE's data placement engine
//     bool success = true;
//     if (step > 0) {
//         auto var_locations = db->getAllBlobs(step, rank);
//         for (const auto &location: var_locations) {
//             success &= Hermes->Demote(location.tag_name, location.blob_name);
//         }
// }
//     return success;
    // This is a no-op - CTE will automatically demote based on scoring
    (void)step;  // Suppress unused parameter warning
    return true;
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

  std::string tag_name = "step_" + std::to_string(currentStep)
                              + "_rank" + std::to_string(rank);
  // if two same run happened in one pipeline
  //std::string tag_name =  adiosOutput + "_step_" + std::to_string(currentStep) + "_rank" + std::to_string(rank);
    // Get or create CTE tag using IHermes interface
    if (!hermes_ || !hermes_->GetTag(tag_name)) {
      throw std::runtime_error("Failed to get/create tag: " + tag_name);
    }
// derived part
//  if(m_OpenMode == adios2::Mode::Read){
//      for(int i = 0; i < num_layers; i++) {
 //         Promote(currentStep + lookahead + i);
//      }
//  }
//  if(m_OpenMode == adios2::Mode::Write){
//      for(int i = 0; i < num_layers; i++) {
//          Demote(currentStep - lookahead - i);
 //     }
//  }
  return adios2::StepStatus::OK;
}



//compute the derived variable
void HermesEngine::ComputeDerivedVariables() {
    auto const &m_VariablesDerived = m_IO.GetDerivedVariables();
  auto const &m_Variables = m_IO.GetVariables();
        // parse all derived variables
  if(rank == 0) {
      std::cout << " Parsing " << m_VariablesDerived.size() << " derived variables"
                << std::endl;

  }

     for (const auto& [name, varPtr] : m_VariablesDerived) {
    // identify the variables used in the derived variable
    auto derivedVar = dynamic_cast<adios2::core::VariableDerived *>(varPtr.get());
    std::vector<std::string> varList = derivedVar->VariableNameList();



    // to create a mapping between variable name and the varInfo (dim and data
    // pointer)
      std::map<std::string, adios2::MinVarInfo> nameToVarInfo;
    for (auto varName : varList) {

      auto itVariable = m_Variables.find(varName);
          if (itVariable == m_Variables.end())
            std::cout <<"throw error commented" <<std::endl;
      // extract the dimensions and data for each variable
      adios2::core::VariableBase *varBase = itVariable->second.get();
      auto blob = hermes_->tag->Get(varName);

      adios2::MinBlockInfo blk({0, 0, itVariable->second.get()->m_Start.data(),
                                itVariable->second.get()->m_Count.data(),
                                adios2::MinMaxStruct(), blob.empty() ? nullptr : blob.data()});


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
    // ExpressionString
    std::vector<std::tuple<void *, adios2::Dims, adios2::Dims>>
        DerivedBlockData;

    if (derivedVar->GetDerivedType() !=
        adios2::DerivedVarType::ExpressionString) {
        std::map<std::string, std::unique_ptr<adios2::MinVarInfo>> NameToMVI;
        for (auto &pair : nameToVarInfo) {
            NameToMVI[pair.first] = std::make_unique<adios2::MinVarInfo>(std::move(pair.second));
        }
      DerivedBlockData = derivedVar->ApplyExpression(NameToMVI);
    }

    for (auto derivedBlock : DerivedBlockData) {
#define DEFINE_VARIABLE_PUT(T)       \
  if (adios2::helper::GetDataType<T>() == derivedVar->m_Type) { \
    T* data = static_cast<T *>(std::get<0>(derivedBlock));\
    PutDerived(*derivedVar, data);   \
  }
  ADIOS2_FOREACH_ATTRIBUTE_PRIMITIVE_STDTYPE_1ARG(DEFINE_VARIABLE_PUT)
#undef DEFINE_VARIABLE_PUT
      free(std::get<0>(derivedBlock));
    }

  }

}



void HermesEngine::IncrementCurrentStep() {
  currentStep++;
}

size_t HermesEngine::CurrentStep() const {
  return currentStep;
}

void HermesEngine::EndStep() {
    ComputeDerivedVariables();
//  if (m_OpenMode == adios2::Mode::Write) {
//    if (rank % ppn == 0) {
//      DbOperation db_op(uid, currentStep);
//      client.Mdm_insertRoot(DomainId::GetLocal(), db_op);
//    }
//  }

  // Tag is managed by CTEHermes, no need to reset
  if (hermes_ && hermes_->tag) {
    delete hermes_->tag;
    hermes_->tag = nullptr;
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

  // Obtain the blob from CTE using the filename and variable name
  auto blob = hermes_->tag->Get(Var.m_Name);
  if (blob.empty()) {
    return false; // Blob not found
  }

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
  TRACE_FUNC();
  return reinterpret_cast<T *>(&u);


}

template <typename T>
void HermesEngine::ElementMinMax(adios2::MinMaxStruct &MinMax, void *element) {
  TRACE_FUNC("MinMax operation");
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
  auto start_time = std::chrono::high_resolution_clock::now();
  auto metadata_vector = db->GetAllVariableMetadata(currentStep, rank);
  for (auto &variableMetadata : metadata_vector) {
    DefineVariable(variableMetadata);
  }
  auto end_time = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
  if (rank == 0) {
    std::cout << "Rank 0 - LoadMetadata time: " << duration << " microseconds (step: " << currentStep << ")" << std::endl;
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


template<typename T>
void HermesEngine::DoGetSync_(const adios2::core::Variable<T> &variable,
                              T *values) {
  TRACE_FUNC(variable.m_Name, adios2::ToString(variable.m_Count));
  auto blob = hermes_->tag->Get(variable.m_Name);
  std::string name = variable.m_Name;
#ifdef Meta_enabled
  // add spdlog method to extract the variable metadata

    metaInfo metaInfo(variable, adiosOpType::get, hermes_->tag->name, name, Get_processor_name(), static_cast<int>(getpid()));
    meta_logger_put->info("MetaData: {}", metaInfoToString(metaInfo));
#endif

  if (!blob.empty()) {
    memcpy(values, blob.data(), blob.size());
  }

}





template<typename T>
void HermesEngine::DoGetDeferred_(
    const adios2::core::Variable<T> &variable, T *values) {

  TRACE_FUNC(variable.m_Name, adios2::ToString(variable.m_Count));
  auto blob = hermes_->tag->Get(variable.m_Name);
  std::string name = variable.m_Name;
#ifdef Meta_enabled
  // add spdlog method to extract the variable metadata
    metaInfo metaInfo(variable, adiosOpType::get, hermes_->tag->name, name, Get_processor_name(), static_cast<int>(getpid()));
    meta_logger_put->info("MetaData: {}", metaInfoToString(metaInfo));
#endif
  //finish metadata extraction
  if (!blob.empty()) {
    memcpy(values, blob.data(), blob.size());
  }

}

//    }



template<typename T>
void HermesEngine::DoPutSync_(const adios2::core::Variable<T> &variable,
                              const T *values) {
  TRACE_FUNC(variable.m_Name, adios2::ToString(variable.m_Count));

  std::string name = variable.m_Name;
  hermes_->tag->Put(name, variable.SelectionSize() * sizeof(T), values);
 


#ifdef Meta_enabled
  metaInfo metaInfo(variable, adiosOpType::put);
  meta_logger_put->info("metadata sync: {}", metaInfoToString(metaInfo));

#endif

  // database
  VariableMetadata vm(variable.m_Name, variable.m_Shape, variable.m_Start,
                      variable.m_Count, variable.IsConstantDims(), true,
                      adios2::ToString(variable.m_Type));
  BlobInfo blobInfo(hermes_->tag->name, name);

  auto start_time_md = std::chrono::high_resolution_clock::now();
  DbOperation db_op(currentStep, rank, std::move(vm), name, std::move(blobInfo));
  client.Mdm_insert(chi::PoolQuery::Local(), db_op);
  auto end_time_md = std::chrono::high_resolution_clock::now();
  auto duration_md = std::chrono::duration_cast<std::chrono::microseconds>(end_time_md - start_time_md).count();
    if (rank == 0) {
    std::cout << "Rank 0 - Mdm_insert (DoPutSync) time: " << duration_md << " microseconds (step: " << currentStep << ", var: " << name << ")" << std::endl;
  }

#ifdef Meta_enabled
    metaInfo metaInfo(variable, adiosOpType::put, hermes_->tag->name, name, Get_processor_name(), static_cast<int>(getpid()));
    meta_logger_put->info("MetaData: {}", metaInfoToString(metaInfo));
#endif
}


template<typename T>
void HermesEngine::DoPutDeferred_(
    const adios2::core::Variable<T> &variable, const T *values) {
  TRACE_FUNC(variable.m_Name, adios2::ToString(variable.m_Count));
  std::string name = variable.m_Name;

  auto start_time = std::chrono::high_resolution_clock::now();
  hermes_->tag->Put(name, variable.SelectionSize() * sizeof(T), values);
  auto end_time = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
  if (rank == 0) {
    std::cout << "Rank 0 - hermes_->tag->Put (DoPutDeferred) time: " << duration << " microseconds (step: " << currentStep << ", var: " << name << ")" << std::endl;
  }
  // database
  VariableMetadata vm(variable.m_Name, variable.m_Shape, variable.m_Start,
                      variable.m_Count, variable.IsConstantDims(), true,
                      adios2::ToString(variable.m_Type));
  BlobInfo blobInfo(hermes_->tag->name, name);
  auto start_time_md = std::chrono::high_resolution_clock::now();
  DbOperation db_op(currentStep, rank, std::move(vm), name, std::move(blobInfo));
  client.Mdm_insert(chi::PoolQuery::Local(), db_op);
  auto end_time_md = std::chrono::high_resolution_clock::now();
  auto duration_md = std::chrono::duration_cast<std::chrono::microseconds>(end_time_md - start_time_md).count();
  if (rank == 0) {
    std::cout << "Rank 0 - Mdm_insert (DoPutDeferred) time: " << duration_md << " microseconds (step: " << currentStep << ", var: " << name << ")" << std::endl;
  }
#ifdef Meta_enabled
    metaInfo metaInfo(variable, adiosOpType::put, hermes_->tag->name, name, Get_processor_name(), static_cast<int>(getpid()));
    meta_logger_put->info("MetaData: {}", metaInfoToString(metaInfo));
#endif


}




template <typename T>
void HermesEngine::PutDerived(adios2::core::VariableDerived variable,
                                  T *values) {
    std::string name = variable.m_Name;
    int total_count = 1;
    for (auto count: variable.m_Count) {
        total_count *= count;
    }
    auto start_time = std::chrono::high_resolution_clock::now();
    hermes_->tag->Put(name, total_count * sizeof(T), values);
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
    if (rank == 0) {
      std::cout << "Rank 0 - hermes_->tag->Put (PutDerived) time: " << duration << " microseconds (step: " << currentStep << ", var: " << variable.m_Name << ")" << std::endl;
    }
    auto start_time_md = std::chrono::high_resolution_clock::now();
    DbOperation db_op = generateMetadata(variable, (float *) values, total_count);
    client.Mdm_insert(chi::PoolQuery::Local(), db_op);
    auto end_time_md = std::chrono::high_resolution_clock::now();
    auto duration_md = std::chrono::duration_cast<std::chrono::microseconds>(end_time_md - start_time_md).count();
    if (rank == 0) {
      std::cout << "Rank 0 - Mdm_insert (PutDerived) time: " << duration_md << " microseconds (step: " << currentStep << ", var: " << variable.m_Name << ")" << std::endl;
    }

}







template<typename T>
DbOperation HermesEngine::generateMetadata(adios2::core::Variable<T> variable) {
  VariableMetadata vm(variable.m_Name, variable.m_Shape, variable.m_Start,
                      variable.m_Count, variable.IsConstantDims(), true,
                      adios2::ToString(variable.m_Type));
  BlobInfo blobInfo(hermes_->tag->name, variable.m_Name);
  return DbOperation(currentStep, rank, std::move(vm), variable.m_Name, std::move(blobInfo));
}

DbOperation HermesEngine::generateMetadata(adios2::core::VariableDerived variable, float *values, int total_count) {
  VariableMetadata vm(variable.m_Name, variable.m_Shape, variable.m_Start,
                     variable.m_Count, variable.IsConstantDims(), true,
                     adios2::ToString(variable.m_Type));
  BlobInfo blobInfo(hermes_->tag->name, variable.m_Name);
    return DbOperation(currentStep, rank, std::move(vm), variable.m_Name, std::move(blobInfo));

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
