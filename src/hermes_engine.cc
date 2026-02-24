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
// IMPORTANT: Include HermesEngine.h (ADIOS2) before CatalystHelper.h (Catalyst/Conduit)
// to avoid preprocessor macro collisions with adios2::core::Variable<T>.
#include "coeus/HermesEngine.h"
#include "common/CatalystHelper.h"
#include "comms/CTEHermes.h"
#include <chimaera/chimaera.h>
#include <chimaera/module_manager.h>
#include <chimaera/ipc_manager.h>
#include <chrono>
#include <cstdlib>
#include <cstring>



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
  // CTE requires Chimaera to be initialized first (WRP_CTE_CLIENT_INIT calls
  // CHIMAERA_INIT internally; initializing here gives a clear error if runtime is down).
  if (!chi::CHIMAERA_INIT(chi::ChimaeraMode::kClient, false)) {
    std::cout << "ERROR: Could not initialize Chimaera (required for CTE)" << std::endl;
    std::cout << "This usually means:" << std::endl;
    std::cout << "  1. Chimaera runtime is not running - start it with: chimaera_start_runtime" << std::endl;
    std::cout << "  2. Port 5555 is already in use by another process" << std::endl;
    std::cout << "  3. Cannot connect to existing Chimaera runtime" << std::endl;
    std::cout << "Solutions:" << std::endl;
    std::cout << "  - Start runtime separately: chimaera_start_runtime" << std::endl;
    std::cout << "  - Check if runtime is running: check port 5555" << std::endl;
    throw coeus::common::ErrorException(HERMES_CONNECT_FAILED);
  }
  hermes_ = new coeus::CTEHermes();
  // Initialize CTE via CTEHermes::connect() (creates/attaches to CTE pool)
  if (!hermes_->connect()) {
    delete hermes_;
    hermes_ = nullptr;
    throw std::runtime_error("Failed to initialize CTE via CTEHermes::connect(). "
                            "Ensure Chimaera runtime is running (see error above or start with chimaera_start_runtime).");
  }
  Init_();
}

/**
 * Test initializer
 * */
HermesEngine::HermesEngine(std::shared_ptr<coeus::MPI> mpi,
                           adios2::core::IO &io, const std::string &name,
                           const adios2::Mode mode, adios2::helper::Comm comm)
    : adios2::plugin::PluginEngineInterface(io, name, mode, comm.Duplicate()) {
  if (!chi::CHIMAERA_INIT(chi::ChimaeraMode::kClient, false)) {
    std::cout << "ERROR: Could not initialize Chimaera (required for CTE)" << std::endl;
    std::cout << "This usually means:" << std::endl;
    std::cout << "  1. Chimaera runtime is not running - start it with: chimaera_start_runtime" << std::endl;
    std::cout << "  2. Port 5555 is already in use by another process" << std::endl;
    std::cout << "  3. Cannot connect to existing Chimaera runtime" << std::endl;
    std::cout << "Solutions:" << std::endl;
    std::cout << "  - Start runtime separately: chimaera_start_runtime" << std::endl;
    std::cout << "  - Check if runtime is running: check port 5555" << std::endl;
    throw coeus::common::ErrorException(HERMES_CONNECT_FAILED);
  }
  hermes_ = new coeus::CTEHermes();
  if (!hermes_->connect()) {
    delete hermes_;
    hermes_ = nullptr;
    throw std::runtime_error("Failed to initialize CTE via CTEHermes::connect(). "
                            "Ensure Chimaera runtime is running (see error above or start with chimaera_start_runtime).");
  }
  Init_();
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

  // Chimaera is already initialized in the constructor (before CTE connect).
  // This call is idempotent; we use default_with_runtime=false so runtime is
  // started separately (chimaera_start_runtime or CHI_WITH_RUNTIME=1).
  const char* with_runtime_env = std::getenv("CHI_WITH_RUNTIME");
  if (!with_runtime_env) {
    with_runtime_env = std::getenv("CHIMAERA_WITH_RUNTIME");  // legacy
  }
  if (with_runtime_env && (std::strcmp(with_runtime_env, "1") == 0 ||
                           std::strcmp(with_runtime_env, "true") == 0 ||
                           std::strcmp(with_runtime_env, "TRUE") == 0)) {
    std::cout << "WARNING: CHI_WITH_RUNTIME=1 (or CHIMAERA_WITH_RUNTIME=1) is set. "
              << "This will start runtime on every MPI rank." << std::endl;
    std::cout << "  This may cause port conflicts. Consider unsetting it or starting runtime separately." << std::endl;
  }
  if (!chi::CHIMAERA_INIT(chi::ChimaeraMode::kClient, false)) {
    std::cout << "ERROR: Could not initialize Chimaera" << std::endl;
    std::cout << "This usually means:" << std::endl;
    std::cout << "  1. Chimaera runtime is not running - start it with: chimaera_start_runtime" << std::endl;
    std::cout << "  2. Port 5555 is already in use by another process" << std::endl;
    std::cout << "  3. Cannot connect to existing Chimaera runtime" << std::endl;
    std::cout << "Solutions:" << std::endl;
    std::cout << "  - Start runtime separately: chimaera_start_runtime" << std::endl;
    std::cout << "  - Check if runtime is running: check port 5555" << std::endl;
    throw coeus::common::ErrorException(HERMES_CONNECT_FAILED);
  }
  std::cout << "Initialized Chimaera (client mode)" << std::endl;

  // Get MPI rank/size from ADIOS2 communicator for coordinating pool creation.
  // Only one rank should create pools to avoid flooding the runtime with
  // duplicate Create tasks (which causes 30s "SendIn task timed out" errors).
  int mpi_rank = m_Comm.Rank();

  // Verify required ChiMod modules are discoverable (diagnostic only)
  if (CHI_MODULE_MANAGER) {
    if (!CHI_MODULE_MANAGER->IsInitialized()) {
      CHI_MODULE_MANAGER->ServerInit();
    }
    auto* rankConsensus_mod = CHI_MODULE_MANAGER->GetChiMod("chimaera_rankConsensus");
    auto* coeus_mdm_mod = CHI_MODULE_MANAGER->GetChiMod("chimaera_coeus_mdm");
    if (mpi_rank == 0) {
      if (rankConsensus_mod) {
        engine_logger->info("rankConsensus module loaded: {}", rankConsensus_mod->lib_path);
      } else {
        engine_logger->warn("rankConsensus module not found - may fail during pool creation");
      }
      if (coeus_mdm_mod) {
        engine_logger->info("coeus_mdm module loaded: {}", coeus_mdm_mod->lib_path);
      } else {
        engine_logger->warn("coeus_mdm module not found - may fail during pool creation");
      }
    }
  }

  // NOTE: The admin pool is built-in to the Chimaera runtime.
  // Do NOT create it from client mode -- it already exists.

  // Initialize rank consensus pool (only MPI rank 0 creates it)
  rankConsensus_pool_id_ = chi::PoolId(8001, 0);
  rank_consensus = chimaera::rankConsensus::Client(rankConsensus_pool_id_);
  if (mpi_rank == 0) {
    rank_consensus.Create(chi::PoolQuery::Dynamic(), "rankConsensus", rankConsensus_pool_id_);
    std::cout << "Rank 0: rankConsensus pool created" << std::endl;
  }
  m_Comm.Barrier("Init_:rankConsensus_pool_created");
  if (mpi_rank != 0) {
    rank_consensus.Init(rankConsensus_pool_id_);
  }
  rank = rank_consensus.GetRank(chi::PoolQuery::Local());
  
  std::cout << "MPI rank " << mpi_rank << " -> consensus rank: " << rank << std::endl;
 
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

  // Chimaera setup for metadata management (coeus_mdm)
  if (params.find("db_file") != params.end()) {
    db_file = params["db_file"];
    db = new SQLiteWrapper(db_file);
    coeus_mdm_pool_id_ = chi::PoolId(8000, 0);
    client = chimaera::coeus_mdm::Client(coeus_mdm_pool_id_);
    if (mpi_rank == 0) {
      client.Create(chi::PoolQuery::Dynamic(), "db_operation", coeus_mdm_pool_id_, db_file);
    }
    m_Comm.Barrier("Init_:coeus_mdm_pool_created");
    if (mpi_rank != 0) {
      client.Init(coeus_mdm_pool_id_);
    }
    if (rank % ppn == 0) {
      db->createTables();
    }
  }

  // Synchronize before Catalyst/Inline setup so all ranks enter together.
  // Otherwise ranks that skip createTables() can reach Open()/catalyst_initialize()
  // while others are still in createTables(); if those calls are collective, we deadlock.
  m_Comm.Barrier("Init_:before_catalyst");

  // if(params.find("execution_order") != params.end()) {
  //     adiosOutput = params["execution_order"];
  // }
  #ifdef COEUS_HAVE_CATALYST
  // Optional Catalyst/Fides activation if parameters provided
  bool enableCatalyst = (params.find("Script") != params.end()) && (params.find("DataModel") != params.end());
  if (enableCatalyst)
  {
    CatalystState = std::unique_ptr<CatalystImpl>(new CatalystImpl());
    CatalystState->ScriptFileName = params["Script"];
    CatalystState->JSONFileName = params["DataModel"];
    if (params.find("CatalystStream") != params.end()) {
      CatalystState->CatalystStreamName = params["CatalystStream"];
    }

    const auto &varMap = m_IO.GetVariables();

    if (!CatalystState->CatalystStreamName.empty())
    {
      // Multi-node: use SST so a separate Catalyst reader can connect
      CatalystState->SSTIO = &m_IO.m_ADIOS.DeclareIO("CatalystSSTIO");
      CatalystState->SSTIO->SetEngine("SST");
      CatalystState->SSTIO->SetParameter("RendezvousReaderCount", "1");
      CatalystState->SSTIO->SetParameter("QueueLimit", "1");
      CatalystState->SSTIO->SetParameter("QueueFullPolicy", "Discard");
      CatalystState->SSTIO->SetParameter("OpenTimeoutSecs", "60.0");
      if (params.find("SSTDataTransport") != params.end()) {
        CatalystState->SSTIO->SetParameter("DataTransport", params["SSTDataTransport"]);
      } else {
        CatalystState->SSTIO->SetParameter("DataTransport", "MPI");
      }

      for (const auto &it : varMap)
      {
   #define declare_type_sst(T) \
     if (it.second->m_Type == adios2::helper::GetDataType<T>()) \
     { \
       CatalystState->SSTIO->DefineVariable<T>(it.first, it.second->m_Shape, it.second->m_Start, \
         it.second->m_Count, it.second->IsConstantDims()); \
       continue; \
     }
        ADIOS2_FOREACH_STDTYPE_1ARG(declare_type_sst)
   #undef declare_type_sst
      }

      CatalystState->SSTWriter = &CatalystState->SSTIO->Open(
          CatalystState->CatalystStreamName, adios2::Mode::Write);
      if (rank == 0) {
        engine_logger->info("Catalyst SST stream: {} (multi-node)", CatalystState->CatalystStreamName);
      }
    }
    else
    {
      // Single-node: Inline engine (Catalyst reads in-process)
      CatalystState->InlineIO = &m_IO.m_ADIOS.DeclareIO("InlinePluginIO");
      CatalystState->InlineIO->SetEngine("inline");

      for (const auto &it : varMap)
      {
   #define declare_type(T) \
     if (it.second->m_Type == adios2::helper::GetDataType<T>()) \
     { \
       CatalystState->InlineIO->DefineVariable<T>(it.first, it.second->m_Shape, it.second->m_Start, \
         it.second->m_Count, it.second->IsConstantDims()); \
       continue; \
     }
        ADIOS2_FOREACH_STDTYPE_1ARG(declare_type)
   #undef declare_type
      }

      CatalystState->InlineWriter = &CatalystState->InlineIO->Open("write", adios2::Mode::Write);
      CatalystInit();
    }
  }
  #endif

  // Synchronize all ranks after pool setup and optional Catalyst/Inline init.
  // Prevents deadlock when the application (or ADIOS2) performs a collective
  // immediately after opening the engine (e.g. first BeginStep or Put).
  m_Comm.Barrier("Init_:setup_complete");

  open = true;

}

/**
 * Close the Engine.
 * */
void HermesEngine::DoClose(const int transportIndex) {
  TRACE_FUNC("engine close");
  #ifdef COEUS_HAVE_CATALYST
  if (CatalystState && CatalystState->CatalystWriter())
  {
    CatalystState->CatalystWriter()->Close(transportIndex);
  }
  #endif
  // Clear tag on close (match IowarpEngine: current_tag_.reset() in DoClose)
  // if (hermes_ && hermes_->tag) {
  //   delete hermes_->tag;
  //   hermes_->tag = nullptr;
  // }
  
  open = false;
}

HermesEngine::~HermesEngine() {
  TRACE_FUNC();
  #ifdef COEUS_HAVE_CATALYST
  if (CatalystState && !CatalystState->UseSST())
  {
    conduit_cpp::Node node;
    catalyst_finalize(conduit_cpp::c_node(&node));
  }
  #endif
  delete db;
  db = nullptr;
  if (hermes_) {
    delete hermes_;
    hermes_ = nullptr;
  }
  if (CHI_IPC) {
    CHI_IPC->ClientFinalize();
  }
}

/**
 * Handle step operations.
 * */

bool HermesEngine::Promote(int step){
    if (!hermes_) {
      engine_logger->error("Promote: CTE not connected");
      return false;
    }

    std::string tag_name = "step_" + std::to_string(step)
                                + "_rank" + std::to_string(rank);

    auto metadata_vector = db->GetAllVariableMetadata(step, rank);
    bool success = true;
    for (auto &variableMetadata : metadata_vector) {
      if (!hermes_->Prefetch(tag_name, variableMetadata.name)) {
        engine_logger->warn("Promote: Prefetch failed for blob '{}' in tag '{}'",
                            variableMetadata.name, tag_name);
        success = false;
      }
    }
    return success;
}

bool HermesEngine::Demote(int step){
    if (!hermes_) {
      engine_logger->error("Demote: CTE not connected");
      return false;
    }

    std::string tag_name = "step_" + std::to_string(step)
                                + "_rank" + std::to_string(rank);

    auto metadata_vector = db->GetAllVariableMetadata(step, rank);
    bool success = true;
    for (auto &variableMetadata : metadata_vector) {
      if (!hermes_->Demote(tag_name, variableMetadata.name)) {
        engine_logger->warn("Demote: Demote failed for blob '{}' in tag '{}'",
                            variableMetadata.name, tag_name);
        success = false;
      }
    }
    return success;
}

adios2::StepStatus HermesEngine::BeginStep(adios2::StepMode mode,
                                           const float timeoutSeconds) {
  IncrementCurrentStep();

  #ifdef COEUS_HAVE_CATALYST
  inline_writer_in_step_ = false;

  if (CatalystState && CatalystState->CatalystWriter())
  {
    try
    {
      adios2::StepStatus status =
          CatalystState->CatalystWriter()->BeginStep(mode, timeoutSeconds);

      inline_writer_in_step_ = (status == adios2::StepStatus::OK);
    }
    catch (...)
    {
      inline_writer_in_step_ = false;
      throw;
    }
  }
  #endif
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
    if (!hermes_) {
      throw std::runtime_error("BeginStep: hermes_ is null (CTE not initialized)");
    }
    if (!hermes_->GetTag(tag_name)) {
      throw std::runtime_error("BeginStep: Failed to get/create tag '" + tag_name
                               + "'. Check that the CTE core runtime is running "
                               "and the CTE pool is properly deployed.");
    }

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

void HermesEngine::EndStep()
{
  #ifdef COEUS_HAVE_CATALYST
  adios2::core::Engine *catWriter = CatalystState ? CatalystState->CatalystWriter() : nullptr;
  bool catalyst_active = (catWriter && inline_writer_in_step_);
  bool use_inline = CatalystState && !CatalystState->UseSST();
  #endif

  try
  {
    ComputeDerivedVariables();
  }
  catch (...)
  {
  #ifdef COEUS_HAVE_CATALYST
    if (catalyst_active && catWriter)
    {
      catWriter->EndStep();
      inline_writer_in_step_ = false;
    }
  #endif
    throw;
  }

  #ifdef COEUS_HAVE_CATALYST
   if (catalyst_active && catWriter)
   {
    catWriter->EndStep();
    if (use_inline) {
      CatalystExecute();
    }
    inline_writer_in_step_ = false;
   }
  #endif

  if (hermes_ && hermes_->tag)
  {
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
  if (!blob.empty()) {
    memcpy(values, blob.data(), blob.size());
  }

}



template<typename T>
void HermesEngine::DoPutSync_(const adios2::core::Variable<T> &variable,
                              const T *values) {
  TRACE_FUNC(variable.m_Name, adios2::ToString(variable.m_Count));
  #ifdef COEUS_HAVE_CATALYST
   if (CatalystState && CatalystState->CatalystWriter())
   {
     adios2::core::IO *catIO = CatalystState->UseSST() ? CatalystState->SSTIO : CatalystState->InlineIO;
     adios2::core::Variable<T> *catVar = catIO->InquireVariable<T>(variable.m_Name);
     if (catVar)
     {
       CatalystState->CatalystWriter()->Put(*catVar, values);
     }
   }
  #endif
  std::string name = variable.m_Name;
  const size_t blob_size = variable.SelectionSize() * sizeof(T);
  if (!hermes_->Put(name, blob_size, values)) {
    throw std::runtime_error("HermesEngine::DoPutSync_: Put failed for " + name);
  }
  // database
  VariableMetadata vm(variable.m_Name, variable.m_Shape, variable.m_Start,
                      variable.m_Count, variable.IsConstantDims(), true,
                      adios2::ToString(variable.m_Type));
  BlobInfo blobInfo(hermes_->tag->name, name);
  //DbOperation db_op(currentStep, rank, std::move(vm), name, std::move(blobInfo));
  //client.Mdm_insert(chi::PoolQuery::Local(), db_op);

}


template<typename T>
void HermesEngine::DoPutDeferred_(
    const adios2::core::Variable<T> &variable, const T *values) {
  TRACE_FUNC(variable.m_Name, adios2::ToString(variable.m_Count));
  std::string name = variable.m_Name;
  const size_t blob_size = variable.SelectionSize() * sizeof(T);
  #ifdef COEUS_HAVE_CATALYST
   if (CatalystState && CatalystState->CatalystWriter())
   {
     adios2::core::IO *catIO = CatalystState->UseSST() ? CatalystState->SSTIO : CatalystState->InlineIO;
     adios2::core::Variable<T> *catVar = catIO->InquireVariable<T>(variable.m_Name);
     if (catVar)
     {
       CatalystState->CatalystWriter()->Put(*catVar, values);
     }
   }
  #endif

  if (!hermes_->Put(name, blob_size, values)) {
    throw std::runtime_error("HermesEngine::DoPutDeferred_: Put failed for " + name);
  }
  // database
  VariableMetadata vm(variable.m_Name, variable.m_Shape, variable.m_Start,
                      variable.m_Count, variable.IsConstantDims(), true,
                      adios2::ToString(variable.m_Type));
  BlobInfo blobInfo(hermes_->tag->name, name);
  //DbOperation db_op(currentStep, rank, std::move(vm), name, std::move(blobInfo));
  //client.Mdm_insert(chi::PoolQuery::Local(), db_op);


}




template <typename T>
void HermesEngine::PutDerived(adios2::core::VariableDerived variable,
                                  T *values) {
    std::string name = variable.m_Name;
    int total_count = 1;
    for (auto count: variable.m_Count) {
        total_count *= count;
    }
    if (!hermes_->Put(name, total_count * sizeof(T), values)) {
      throw std::runtime_error("HermesEngine::PutDerived: Put failed for " + name);
    }
    DbOperation db_op = generateMetadata(variable, (float *) values, total_count);
    client.Mdm_insert(chi::PoolQuery::Local(), db_op);

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



}
 // namespace coeus
// Catalyst helper function implementations (CatalystConfig, CatalystInit, CatalystExecute)

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
