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
#include <clio_runtime/clio_runtime.h>
#include <clio_runtime/module_manager.h>
#include <clio_runtime/ipc_manager.h>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <thread>

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
  // CTE requires Chimaera to be initialized first (CLIO_CTE_CLIENT_INIT calls
  // CHIMAERA_INIT internally; initializing here gives a clear error if runtime is down).
  if (!clio::run::CLIO_INIT(clio::run::RuntimeMode::kClient, false)) {
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
  if (!clio::run::CLIO_INIT(clio::run::RuntimeMode::kClient, false)) {
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
  if (!clio::run::CLIO_INIT(clio::run::RuntimeMode::kClient, false)) {
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
  if (CLIO_MODULE_MANAGER) {
    if (!CLIO_MODULE_MANAGER->IsInitialized()) {
      CLIO_MODULE_MANAGER->ServerInit();
    }
    auto* rankConsensus_mod = CLIO_MODULE_MANAGER->GetChiMod("coeus_rankConsensus");
    auto* coeus_mdm_mod = CLIO_MODULE_MANAGER->GetChiMod("coeus_coeus_mdm");
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
  rankConsensus_pool_id_ = clio::run::PoolId(8001, 0);
  rank_consensus = coeus::rankConsensus::Client(rankConsensus_pool_id_);
  if (mpi_rank == 0) {
    rank_consensus.Create(clio::run::PoolQuery::Dynamic(), "rankConsensus", rankConsensus_pool_id_);
    std::cout << "Rank 0: rankConsensus pool created" << std::endl;
  }
  m_Comm.Barrier("Init_:rankConsensus_pool_created");
  if (mpi_rank != 0) {
    rank_consensus.Init(rankConsensus_pool_id_);
  }
  // The consensus pool keeps one atomic counter per container (per node),
  // so querying it from every process hands out per-node ranks: the
  // step_N_rankR CTE tags then collide across nodes and ranks read and
  // overwrite each other's blobs (corrupted derived block means / trigger
  // statistics). Instead, only MPI rank 0 draws one value per run (the
  // across-runs uniquifier on a persistent runtime) and broadcasts it as a
  // run base; every process derives a globally unique rank from it and its
  // MPI rank. On a fresh runtime the base is 0, so rank == MPI rank.
  int run_base = 0;
  if (mpi_rank == 0) {
    run_base = rank_consensus.GetRank(clio::run::PoolQuery::Local());
  }
  run_base = m_Comm.BroadcastValue(run_base, 0);
  rank = run_base * 1000000 + mpi_rank;


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

  // Statistical trigger configuration (Vigil trigger phase).
  // Knobs shared by all trigger types:
  if (params.find("TriggerType") != params.end()) {
    trigger_type_ = params["TriggerType"];
  }
  if (params.find("TriggerInspectSteps") != params.end()) {
    trigger_inspect_steps_ = std::max(1, std::stoi(params["TriggerInspectSteps"]));
  }
  if (params.find("TriggerRefire") != params.end()) {
    const std::string &v = params["TriggerRefire"];
    trigger_refire_ = (v == "1" || v == "true" || v == "TRUE" || v == "True");
  }
  if (params.find("TriggerLogFile") != params.end()) {
    trigger_log_file_ = params["TriggerLogFile"];
  }
  if (params.find("TriggerMetricsLogFile") != params.end()) {
    trigger_metrics_log_file_ = params["TriggerMetricsLogFile"];
  }

  if (trigger_type_ == "dissipation") {
    // Two-stage Yellow/Red numerical-dissipation trigger (Xcompact3d TGV).
    if (params.find("TriggerKEVariable") != params.end()) {
      trigger_ke_variable_ = params["TriggerKEVariable"];
    }
    if (params.find("TriggerEnstrophyVariable") != params.end()) {
      trigger_enst_variable_ = params["TriggerEnstrophyVariable"];
    }
    if (params.find("TriggerNu") != params.end()) {
      trigger_nu_ = std::stod(params["TriggerNu"]);
    }
    if (params.find("TriggerOutputDt") != params.end()) {
      trigger_output_dt_ = std::stod(params["TriggerOutputDt"]);
    }
    if (params.find("TriggerYellowFraction") != params.end()) {
      trigger_yellow_fraction_ = std::stod(params["TriggerYellowFraction"]);
    }
    if (params.find("TriggerRedFraction") != params.end()) {
      trigger_red_fraction_ = std::stod(params["TriggerRedFraction"]);
    }
    if (params.find("TriggerYellowNuRatio") != params.end()) {
      trigger_yellow_nu_ratio_ = std::stod(params["TriggerYellowNuRatio"]);
    }
    if (params.find("TriggerRedNuRatio") != params.end()) {
      trigger_red_nu_ratio_ = std::stod(params["TriggerRedNuRatio"]);
    }
    trigger_enabled_ = (!trigger_ke_variable_.empty() &&
                        !trigger_enst_variable_.empty() &&
                        trigger_nu_ > 0.0 && trigger_output_dt_ > 0.0);
    if (mpi_rank == 0) {
      if (trigger_enabled_) {
        std::cout << "Trigger: dissipation ke=" << trigger_ke_variable_
                  << " enst=" << trigger_enst_variable_
                  << " nu=" << trigger_nu_
                  << " output_dt=" << trigger_output_dt_
                  << " yellow(frac=" << trigger_yellow_fraction_
                  << ",nu_ratio=" << trigger_yellow_nu_ratio_ << ")"
                  << " red(frac=" << trigger_red_fraction_
                  << ",nu_ratio=" << trigger_red_nu_ratio_ << ")"
                  << " inspect_steps=" << trigger_inspect_steps_
                  << " refire=" << (trigger_refire_ ? "true" : "false") << std::endl;
      } else {
        std::cout << "Trigger: TriggerType=dissipation needs TriggerKEVariable,"
                     " TriggerEnstrophyVariable, TriggerNu and TriggerOutputDt"
                     " - trigger disabled" << std::endl;
      }
    }
  } else if (params.find("TriggerVariable") != params.end()) {
    trigger_variable_ = params["TriggerVariable"];
    if (params.find("TriggerSumVariable") != params.end()) {
      trigger_sum_variable_ = params["TriggerSumVariable"];
    }
    if (params.find("TriggerThreshold") != params.end()) {
      trigger_threshold_ = std::stod(params["TriggerThreshold"]);
    }
    if (params.find("TriggerBaselineRatio") != params.end()) {
      trigger_baseline_ratio_ = std::stod(params["TriggerBaselineRatio"]);
    }
    trigger_enabled_ = (trigger_threshold_ > 0.0 || trigger_baseline_ratio_ > 0.0);
    if (mpi_rank == 0) {
      if (trigger_enabled_) {
        std::cout << "Trigger: variance(" << trigger_variable_ << ")"
                  << (trigger_sum_variable_.empty()
                          ? ""
                          : " sum_var=" + trigger_sum_variable_)
                  << " threshold=" << trigger_threshold_
                  << " baseline_ratio=" << trigger_baseline_ratio_
                  << " inspect_steps=" << trigger_inspect_steps_
                  << " refire=" << (trigger_refire_ ? "true" : "false") << std::endl;
      } else {
        std::cout << "Trigger: TriggerVariable set but no TriggerThreshold/"
                     "TriggerBaselineRatio - trigger disabled" << std::endl;
      }
    }
  }

  // Chimaera setup for metadata management (coeus_mdm)
  if (params.find("db_file") != params.end()) {
    db_file = params["db_file"];
    db = new SQLiteWrapper(db_file);
    coeus_mdm_pool_id_ = clio::run::PoolId(8000, 0);
    client = coeus::coeus_mdm::Client(coeus_mdm_pool_id_);
    if (mpi_rank == 0) {
      client.Create(clio::run::PoolQuery::Dynamic(), "db_operation", coeus_mdm_pool_id_, db_file);
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
      // Trigger-gated streams ship only flagged steps, which must not be
      // dropped; ungated streams keep the historical Discard behavior.
      std::string queue_policy = trigger_enabled_ ? "Block" : "Discard";
      if (params.find("SSTQueueFullPolicy") != params.end()) {
        queue_policy = params["SSTQueueFullPolicy"];
      }
      CatalystState->SSTIO->SetParameter("QueueFullPolicy", queue_policy);
      CatalystState->SSTIO->SetParameter("OpenTimeoutSecs", "60.0");
      if (params.find("SSTDataTransport") != params.end()) {
        CatalystState->SSTIO->SetParameter("DataTransport", params["SSTDataTransport"]);
      } else {
        CatalystState->SSTIO->SetParameter("DataTransport", "WAN");
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

      if (trigger_enabled_) {
        // Trigger state travels with every shipped step (rank 0 writes them).
        CatalystState->SSTIO->DefineVariable<int32_t>("vigil/trigger_fired");
        CatalystState->SSTIO->DefineVariable<double>("vigil/trigger_stat");
        CatalystState->SSTIO->DefineVariable<int32_t>("vigil/trigger_fire_step");
      }

      CatalystState->SSTWriter = &CatalystState->SSTIO->Open(
          CatalystState->CatalystStreamName, adios2::Mode::Write, m_Comm.Duplicate());

      if (rank == 0) {
        engine_logger->info("Catalyst SST stream: {} (multi-node{})",
                            CatalystState->CatalystStreamName,
                            trigger_enabled_ ? ", trigger-gated" : "");
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
  int mpi_rank = m_Comm.Rank();
  // Ensure all ranks have completed EndStep before closing SST collectively.
  
  m_Comm.Barrier("DoClose:before_sst_close");
  
  #ifdef COEUS_HAVE_CATALYST
  if (CatalystState && CatalystState->CatalystWriter())
  {
    if (CatalystState->UseSST()) {
      // SST Close() blocks indefinitely waiting for the reader to
      // acknowledge EndOfStream.  Skip it — RemoveIO below destroys
      // the engine (closing TCP sockets), and the reader will detect
      // the disconnect.
      engine_logger->info("DoClose: MPI rank {} skipping SST Close", mpi_rank);
      // Give the reader time to finish processing the last step.
      // Without this delay, destroying the SST engine immediately causes
      // "Writer failed before returning data" abort on the reader side
      // (C++ std::runtime_error that cannot be caught in Python).
      engine_logger->info("DoClose: MPI rank {} waiting 4s for reader to finish last step", mpi_rank);
      std::this_thread::sleep_for(std::chrono::seconds(4));
      engine_logger->info("DoClose: MPI rank {} done waiting, destroying SST engine", mpi_rank);
      // Remove the SST contact file so external watchdogs can detect
      // that the writer has exited.  Only rank 0 needs to do this.
      if (mpi_rank == 0 && !CatalystState->CatalystStreamName.empty()) {
        std::string sst_contact = CatalystState->CatalystStreamName + ".sst";
        if (std::remove(sst_contact.c_str()) == 0) {
          engine_logger->info("DoClose: removed SST contact file '{}'", sst_contact);
        }
      }
      CatalystState->SSTWriter = nullptr;
      CatalystState->SSTIO = nullptr;
      // Remove the IO so the ADIOS2 destructor (which runs after
      // MPI_Finalize) does not attempt a late SST cleanup.
      try { m_IO.m_ADIOS.RemoveIO("CatalystSSTIO"); }
      catch (...) { engine_logger->warn("DoClose: RemoveIO(CatalystSSTIO) failed"); }
    } else {
      CatalystState->CatalystWriter()->Close(transportIndex);
    }
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
  if (CLIO_IPC) {
    CLIO_IPC->ClientFinalize();
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
  if (CatalystState && CatalystState->UseSST()) {
    sst_put_time_us_ = 0;  // Reset per-step SST Put timing for in-transit metrics
  }

  if (CatalystState && CatalystState->CatalystWriter() && !SstGated_())
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
    // Blobs must outlive ApplyExpression below: MinBlockInfo stores raw
    // pointers into these buffers (moving the outer vector is fine, the
    // inner heap buffers stay put).
    std::vector<std::vector<uint8_t>> blobStorage;
    for (auto varName : varList) {

      auto itVariable = m_Variables.find(varName);
          if (itVariable == m_Variables.end())
            std::cout <<"throw error commented" <<std::endl;
      // extract the dimensions and data for each variable
      adios2::core::VariableBase *varBase = itVariable->second.get();
      blobStorage.push_back(hermes_->tag->Get(varName));
      auto &blob = blobStorage.back();

      if (std::getenv("COEUS_DERIVED_DEBUG")) {
        size_t count_prod = 1;
        for (auto c : varBase->m_Count) count_prod *= c;
        size_t blob_elems = blob.size() / sizeof(double);
        double direct_mean = 0.0, direct_min = 0.0, direct_max = 0.0;
        if (blob_elems > 0) {
          const double *d = reinterpret_cast<const double *>(blob.data());
          direct_min = direct_max = d[0];
          for (size_t i = 0; i < blob_elems; i++) {
            direct_mean += d[i];
            direct_min = std::min(direct_min, d[i]);
            direct_max = std::max(direct_max, d[i]);
          }
          direct_mean /= static_cast<double>(blob_elems);
        }
        engine_logger->info(
            "DERIVED_DEBUG mpi_rank={} derived={} src={} shape={} start={} count={} "
            "count_prod={} blob_elems={} blob_mean={} blob_min={} blob_max={}",
            m_Comm.Rank(), name, varName, adios2::ToString(varBase->m_Shape),
            adios2::ToString(varBase->m_Start), adios2::ToString(varBase->m_Count),
            count_prod, blob_elems, direct_mean, direct_min, direct_max);
        if (m_Comm.Rank() == 0 && varName == "ux" && name == "tke_mean" &&
            blob_elems >= 70) {
          const double *d = reinterpret_cast<const double *>(blob.data());
          std::string vals;
          for (int i = 0; i < 70; i++) vals += fmt::format("{:.4f} ", d[i]);
          engine_logger->info("DERIVED_DEBUG ux[0..69]: {}", vals);
        }
      }

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
    if (CatalystState->UseSST())
    {
      auto sst_endstep_t0 = std::chrono::high_resolution_clock::now();
      catWriter->EndStep();
      auto sst_endstep_t1 = std::chrono::high_resolution_clock::now();
      int64_t sst_endstep_us = static_cast<int64_t>(
          std::chrono::duration_cast<std::chrono::microseconds>(
              sst_endstep_t1 - sst_endstep_t0).count());
      if (rank == 0) {
        engine_logger->info(
            "SST in-transit step {}: Put time (us): {}, EndStep time (us): {}",
            currentStep, sst_put_time_us_, sst_endstep_us);
      }
    }
    else
    {
      catWriter->EndStep();
      if (use_inline) {
        CatalystExecute();
      }
    }
    inline_writer_in_step_ = false;
   }
  #endif

  // Statistical trigger: evaluate every step (collective); stream flagged
  // steps over SST from the CTE blobs written earlier in this step.
  if (trigger_enabled_ && m_OpenMode == adios2::Mode::Write &&
      hermes_ && hermes_->tag) {
    bool fired_now = EvaluateTrigger_();
    #ifdef COEUS_HAVE_CATALYST
    if (trigger_window_remaining_ > 0 && CatalystState &&
        CatalystState->UseSST()) {
      StreamFlaggedStepToSST_(fired_now);
    }
    #endif
    if (trigger_window_remaining_ > 0) {
      trigger_window_remaining_--;
    }
  }

  if (hermes_ && hermes_->tag)
  {
    delete hermes_->tag;
    hermes_->tag = nullptr;
  }
}

bool HermesEngine::SstGated_() const {
  #ifdef COEUS_HAVE_CATALYST
  return trigger_enabled_ && CatalystState && CatalystState->UseSST();
  #else
  return false;
  #endif
}

/**
 * Exact pooled global variance of `name` across all ranks (collective).
 *
 * Each rank accumulates (n, sum, sum of squares) over its local block from
 * the CTE blob, a single Allreduce combines them, and the global variance
 * follows as E[x^2] - E[x]^2. This is the pooled-variance formula: per-block
 * variances are never averaged, so the between-block term is fully captured.
 * */
double HermesEngine::ComputeGlobalVariance_(const std::string &name) {
  double local[3] = {0.0, 0.0, 0.0};  // n, sum, sumsq

  auto blob = hermes_->tag->Get(name);
  const adios2::DataType type = m_IO.InquireVariableType(name);
  if (!blob.empty()) {
    if (type == adios2::DataType::Double) {
      const double *data = reinterpret_cast<const double *>(blob.data());
      const size_t n = blob.size() / sizeof(double);
      for (size_t i = 0; i < n; ++i) {
        local[1] += data[i];
        local[2] += data[i] * data[i];
      }
      local[0] = static_cast<double>(n);
    } else if (type == adios2::DataType::Float) {
      const float *data = reinterpret_cast<const float *>(blob.data());
      const size_t n = blob.size() / sizeof(float);
      for (size_t i = 0; i < n; ++i) {
        const double v = static_cast<double>(data[i]);
        local[1] += v;
        local[2] += v * v;
      }
      local[0] = static_cast<double>(n);
    } else if (m_Comm.Rank() == 0) {
      engine_logger->warn("Trigger: variable '{}' has unsupported type '{}'",
                          name, adios2::ToString(type));
    }
  }

  double global[3] = {0.0, 0.0, 0.0};
  m_Comm.Allreduce(local, global, 3, adios2::helper::Comm::Op::Sum);

  if (global[0] <= 0.0) {
    return std::numeric_limits<double>::quiet_NaN();
  }
  const double mean = global[1] / global[0];
  const double var = global[2] / global[0] - mean * mean;
  return var < 0.0 ? 0.0 : var;  // clamp tiny negative round-off
}

/**
 * Sum all elements of `name`'s CTE blob (double or float). Works for raw
 * fields and for derived partial sums (e.g. add(x)): summing the partial
 * sums yields the block total either way. Returns false if the blob is
 * missing or the type is unsupported.
 * */
bool HermesEngine::SumBlob_(const std::string &name, double &sum, double &n) {
  sum = 0.0;
  n = 0.0;

  auto blob = hermes_->tag->Get(name);
  if (blob.empty()) {
    return false;
  }

  adios2::DataType type = adios2::DataType::None;
  auto const &derivedMap = m_IO.GetDerivedVariables();
  auto dit = derivedMap.find(name);
  if (dit != derivedMap.end()) {
    type = dit->second->m_Type;
  } else {
    type = m_IO.InquireVariableType(name);
  }

  if (type == adios2::DataType::Double) {
    const double *data = reinterpret_cast<const double *>(blob.data());
    const size_t count = blob.size() / sizeof(double);
    for (size_t i = 0; i < count; ++i) sum += data[i];
    n = static_cast<double>(count);
    return true;
  }
  if (type == adios2::DataType::Float) {
    const float *data = reinterpret_cast<const float *>(blob.data());
    const size_t count = blob.size() / sizeof(float);
    for (size_t i = 0; i < count; ++i) sum += static_cast<double>(data[i]);
    n = static_cast<double>(count);
    return true;
  }
  return false;
}

/**
 * Pooled global variance from an ADIOS2 derived-quantity variance
 * (collective).
 *
 * The derived operator (variance(x), computed by ComputeDerivedVariables
 * earlier in this EndStep) reduces each rank's block to one local variance
 * var_b. Per VARIANCE_TRIGGER.md §6, var_b alone is not poolable: the exact
 * combine also needs the block mean mean_b and size N_b,
 *
 *   N = sum_b N_b,  mean = (1/N) sum_b N_b*mean_b,
 *   var = (1/N) sum_b N_b*(var_b + mean_b^2) - mean^2.
 *
 * N_b comes from the source field's local Count (no data read); mean_b from
 * the block sum, taken from TriggerSumVariable's derived blob (e.g. add(x))
 * when configured, else from one pass over the raw source blob. A single
 * 3-double Allreduce yields the exact global variance; per-block variances
 * are never averaged.
 * */
double HermesEngine::ComputeGlobalVarianceDerived_(
    adios2::core::VariableDerived *derivedVar) {
  double local[3] = {0.0, 0.0, 0.0};  // N_b, sum_b, N_b*(var_b + mean_b^2)
  bool have_block = false;

  do {
    if (!derivedVar) break;

    // Per-block variance computed by the derived-quantity operator.
    auto var_blob = hermes_->tag->Get(trigger_variable_);
    if (var_blob.empty()) break;
    double var_b;
    if (derivedVar->m_Type == adios2::DataType::Double &&
        var_blob.size() >= sizeof(double)) {
      var_b = *reinterpret_cast<const double *>(var_blob.data());
    } else if (derivedVar->m_Type == adios2::DataType::Float &&
               var_blob.size() >= sizeof(float)) {
      var_b = static_cast<double>(
          *reinterpret_cast<const float *>(var_blob.data()));
    } else {
      break;
    }

    // Block size N_b from the source field named in the derived expression.
    std::vector<std::string> sources = derivedVar->VariableNameList();
    if (sources.empty()) break;
    auto const &varMap = m_IO.GetVariables();
    auto sit = varMap.find(sources.front());
    if (sit == varMap.end()) break;
    double n_b = 1.0;
    for (auto c : sit->second->m_Count) n_b *= static_cast<double>(c);
    if (n_b <= 0.0) break;

    // Block sum for mean_b: derived partial sums if configured, else the
    // raw source blob.
    double sum_b = 0.0, unused = 0.0;
    const std::string &sum_source = trigger_sum_variable_.empty()
                                        ? sources.front()
                                        : trigger_sum_variable_;
    if (!SumBlob_(sum_source, sum_b, unused)) break;

    const double mean_b = sum_b / n_b;
    local[0] = n_b;
    local[1] = sum_b;
    local[2] = n_b * (var_b + mean_b * mean_b);
    have_block = true;
  } while (false);

  if (!have_block && m_Comm.Rank() == 0) {
    engine_logger->warn(
        "Trigger: derived variance '{}' unavailable this step (blob or source"
        " missing) - contributing empty block", trigger_variable_);
  }

  double global[3] = {0.0, 0.0, 0.0};
  m_Comm.Allreduce(local, global, 3, adios2::helper::Comm::Op::Sum);

  if (global[0] <= 0.0) {
    return std::numeric_limits<double>::quiet_NaN();
  }
  const double mean = global[1] / global[0];
  const double var = global[2] / global[0] - mean * mean;
  return var < 0.0 ? 0.0 : var;  // clamp tiny negative round-off
}

/**
 * Evaluate the trigger condition for the current step (collective).
 *
 * Rising-edge semantics: a fire opens an inspect window of
 * trigger_inspect_steps_ steps (the firing step included). By default the
 * trigger fires once per run; TriggerRefire=true re-arms it after the
 * window closes. Returns true only on the step the trigger fires.
 * */
bool HermesEngine::EvaluateTrigger_() {
  if (trigger_type_ == "dissipation") {
    return EvaluateDissipationTrigger_();
  }
  // Prefer the ADIOS2 derived-quantity path: if TriggerVariable names a
  // derived variable (e.g. "derive/VarV" = variance(x)), pool the per-block
  // variances it computed this step; otherwise fall back to a direct pass
  // over the raw field. The branch is rank-uniform (same DefineDerived-
  // Variable calls everywhere), so both paths stay collective.
  double stat;
  auto const &derivedMap = m_IO.GetDerivedVariables();
  auto dit = derivedMap.find(trigger_variable_);
  if (dit != derivedMap.end()) {
    stat = ComputeGlobalVarianceDerived_(
        dynamic_cast<adios2::core::VariableDerived *>(dit->second.get()));
  } else {
    stat = ComputeGlobalVariance_(trigger_variable_);
  }
  trigger_last_stat_ = stat;
  if (std::isnan(stat)) {
    return false;
  }
  if (trigger_baseline_ < 0.0) {
    trigger_baseline_ = stat;
  }

  bool condition = false;
  if (trigger_threshold_ > 0.0 && stat >= trigger_threshold_) {
    condition = true;
  }
  if (!condition && trigger_baseline_ratio_ > 0.0 && trigger_baseline_ > 0.0 &&
      stat >= trigger_baseline_ratio_ * trigger_baseline_) {
    condition = true;
  }

  const bool rising = condition && !trigger_prev_condition_;
  trigger_prev_condition_ = condition;

  if (!rising || trigger_window_remaining_ > 0 ||
      (trigger_has_fired_ && !trigger_refire_)) {
    return false;
  }

  trigger_has_fired_ = true;
  trigger_fire_step_ = currentStep;
  trigger_window_remaining_ = trigger_inspect_steps_;

  // Guard on the MPI rank: the consensus rank is not guaranteed to include 0
  // when the runtime (and its rankConsensus pool) outlives a previous run.
  if (m_Comm.Rank() == 0) {
    engine_logger->info(
        "Trigger FIRED at step {}: variance({}) = {} (threshold {}, baseline {},"
        " ratio {}), streaming {} step(s)",
        currentStep, trigger_variable_, stat, trigger_threshold_,
        trigger_baseline_, trigger_baseline_ratio_, trigger_inspect_steps_);
    std::ofstream log(trigger_log_file_, std::ios::app);
    if (log) {
      log << "{\"event\":\"trigger_fired\",\"step\":" << currentStep
          << ",\"variable\":\"" << trigger_variable_ << "\""
          << ",\"stat\":\"variance\",\"value\":" << stat
          << ",\"threshold\":" << trigger_threshold_
          << ",\"baseline\":" << trigger_baseline_
          << ",\"baseline_ratio\":" << trigger_baseline_ratio_
          << ",\"inspect_steps\":" << trigger_inspect_steps_ << "}\n";
    }
  }
  return true;
}

/**
 * N_b-weighted global mean from an ADIOS2 derived per-block mean (collective).
 *
 * The custom "mean" operator (e.g. tke_mean, enst_mean in Xcompact3d) reduces
 * each rank's block to one value mean_b. The exact global mean is the
 * size-weighted combine  sum_b N_b*mean_b / sum_b N_b,  with N_b taken from
 * the local Count of the first source field named in the derived expression
 * (no field data is read). One 2-double Allreduce; NaN if unavailable.
 * */
double HermesEngine::ComputeGlobalBlockMean_(const std::string &name) {
  double local[2] = {0.0, 0.0};  // N_b, N_b*mean_b
  bool have_block = false;

  do {
    auto const &derivedMap = m_IO.GetDerivedVariables();
    auto dit = derivedMap.find(name);
    if (dit == derivedMap.end()) break;
    auto *derivedVar =
        dynamic_cast<adios2::core::VariableDerived *>(dit->second.get());
    if (!derivedVar) break;

    auto blob = hermes_->tag->Get(name);
    if (blob.empty()) break;
    double mean_b;
    if (derivedVar->m_Type == adios2::DataType::Double &&
        blob.size() >= sizeof(double)) {
      mean_b = *reinterpret_cast<const double *>(blob.data());
    } else if (derivedVar->m_Type == adios2::DataType::Float &&
               blob.size() >= sizeof(float)) {
      mean_b = static_cast<double>(
          *reinterpret_cast<const float *>(blob.data()));
    } else {
      break;
    }

    std::vector<std::string> sources = derivedVar->VariableNameList();
    if (sources.empty()) break;
    auto const &varMap = m_IO.GetVariables();
    auto sit = varMap.find(sources.front());
    if (sit == varMap.end()) break;
    double n_b = 1.0;
    for (auto c : sit->second->m_Count) n_b *= static_cast<double>(c);
    if (n_b <= 0.0) break;

    local[0] = n_b;
    local[1] = n_b * mean_b;
    have_block = true;

    if (std::getenv("COEUS_DERIVED_DEBUG")) {
      engine_logger->info("DERIVED_DEBUG mpi_rank={} pooled_input name={} mean_b={} n_b={}",
                          m_Comm.Rank(), name, mean_b, n_b);
    }
  } while (false);

  if (!have_block && m_Comm.Rank() == 0) {
    engine_logger->warn(
        "Trigger: derived block mean '{}' unavailable this step (variable,"
        " blob or source missing) - contributing empty block", name);
  }

  double global[2] = {0.0, 0.0};
  m_Comm.Allreduce(local, global, 2, adios2::helper::Comm::Op::Sum);

  if (global[0] <= 0.0) {
    return std::numeric_limits<double>::quiet_NaN();
  }
  return global[1] / global[0];
}

/**
 * Two-stage Yellow/Red numerical-dissipation trigger (collective).
 *
 * Pools the derived block-mean TKE and enstrophy, time-differences the TKE
 * against the previous output step to get the total dissipation
 * eps_total = -dE_k/dt, and compares it with the physical dissipation
 * eps_phys = 2*nu*<enstrophy> (exact for periodic flow). The excess is
 * numerical: eps_frac = (eps_total - eps_phys)/eps_total and
 * nu_ratio = eps_total/eps_phys (= nu_eff/nu).
 *
 * Yellow (either yellow threshold crossed) logs an escalation event on its
 * rising edge but does not stream. Red (either red threshold crossed) fires
 * like the variance trigger: rising edge, once by default, opens the SST
 * inspect window. Returns true only on the Red fire step.
 * */
bool HermesEngine::EvaluateDissipationTrigger_() {
  const double ke = ComputeGlobalBlockMean_(trigger_ke_variable_);
  const double enst = ComputeGlobalBlockMean_(trigger_enst_variable_);
  if (std::isnan(ke) || std::isnan(enst)) {
    return false;
  }

  const double eps_phys = 2.0 * trigger_nu_ * enst;
  const bool have_prev = trigger_prev_ke_ >= 0.0;
  double eps_total = std::numeric_limits<double>::quiet_NaN();
  double eps_frac = std::numeric_limits<double>::quiet_NaN();
  double nu_ratio = std::numeric_limits<double>::quiet_NaN();
  if (have_prev) {
    eps_total = (trigger_prev_ke_ - ke) / trigger_output_dt_;
    if (eps_total > 0.0) {
      eps_frac = (eps_total - eps_phys) / eps_total;
      if (eps_frac < 0.0) eps_frac = 0.0;
      nu_ratio = eps_phys > 0.0 ? eps_total / eps_phys : 0.0;
    } else {
      // Energy not decaying (early TGV phase): no dissipation deficit yet.
      eps_frac = 0.0;
      nu_ratio = 0.0;
    }
  }
  trigger_prev_ke_ = ke;
  trigger_last_stat_ = std::isnan(eps_frac) ? 0.0 : eps_frac;

  if (!trigger_metrics_log_file_.empty() && m_Comm.Rank() == 0) {
    std::ofstream mlog(trigger_metrics_log_file_, std::ios::app);
    if (mlog) {
      mlog << "{\"step\":" << currentStep
           << ",\"ke\":" << ke
           << ",\"enstrophy\":" << enst
           << ",\"eps_total\":" << eps_total
           << ",\"eps_phys\":" << eps_phys
           << ",\"eps_frac\":" << eps_frac
           << ",\"nu_ratio\":" << nu_ratio << "}\n";
    }
  }

  if (!have_prev) {
    return false;  // first evaluated step: no dE_k/dt yet
  }

  // NaN comparisons are false, so undefined metrics never escalate.
  const bool yellow = eps_frac >= trigger_yellow_fraction_ ||
                      nu_ratio >= trigger_yellow_nu_ratio_;
  const bool red = eps_frac >= trigger_red_fraction_ ||
                   nu_ratio >= trigger_red_nu_ratio_;

  // Yellow: log-only escalation ("watch carefully") on its rising edge.
  if (yellow && !trigger_yellow_prev_ && m_Comm.Rank() == 0) {
    engine_logger->info(
        "Trigger YELLOW at step {}: eps_frac={} nu_ratio={} (thresholds {}, {})",
        currentStep, eps_frac, nu_ratio, trigger_yellow_fraction_,
        trigger_yellow_nu_ratio_);
    std::ofstream log(trigger_log_file_, std::ios::app);
    if (log) {
      log << "{\"event\":\"trigger_yellow\",\"step\":" << currentStep
          << ",\"ke\":" << ke << ",\"enstrophy\":" << enst
          << ",\"eps_total\":" << eps_total << ",\"eps_phys\":" << eps_phys
          << ",\"eps_frac\":" << eps_frac << ",\"nu_ratio\":" << nu_ratio
          << "}\n";
    }
  }
  trigger_yellow_prev_ = yellow;

  const bool rising = red && !trigger_prev_condition_;
  trigger_prev_condition_ = red;
  if (!rising || trigger_window_remaining_ > 0 ||
      (trigger_has_fired_ && !trigger_refire_)) {
    return false;
  }

  trigger_has_fired_ = true;
  trigger_fire_step_ = currentStep;
  trigger_window_remaining_ = trigger_inspect_steps_;

  if (m_Comm.Rank() == 0) {
    engine_logger->info(
        "Trigger RED at step {}: eps_frac={} nu_ratio={} (thresholds {}, {}),"
        " streaming {} step(s)",
        currentStep, eps_frac, nu_ratio, trigger_red_fraction_,
        trigger_red_nu_ratio_, trigger_inspect_steps_);
    std::ofstream log(trigger_log_file_, std::ios::app);
    if (log) {
      log << "{\"event\":\"trigger_red\",\"step\":" << currentStep
          << ",\"ke\":" << ke << ",\"enstrophy\":" << enst
          << ",\"eps_total\":" << eps_total << ",\"eps_phys\":" << eps_phys
          << ",\"eps_frac\":" << eps_frac << ",\"nu_ratio\":" << nu_ratio
          << ",\"inspect_steps\":" << trigger_inspect_steps_ << "}\n";
    }
  }
  return true;
}

#ifdef COEUS_HAVE_CATALYST
/**
 * Ship the current (flagged) step over SST: every mirrored variable is
 * re-Put from its CTE blob, plus the trigger-state scalars from rank 0.
 * Runs only inside an open inspect window; unflagged steps ship nothing,
 * so the reader simply waits until the next flagged step arrives.
 * */
void HermesEngine::StreamFlaggedStepToSST_(bool fired_now) {
  auto *writer = CatalystState->SSTWriter;
  auto *io = CatalystState->SSTIO;

  auto t0 = std::chrono::high_resolution_clock::now();
  writer->BeginStep(adios2::StepMode::Append, -1.0);

  for (const auto &it : io->GetVariables()) {
    const std::string &name = it.first;
    if (name.rfind("vigil/", 0) == 0) {
      continue;  // trigger scalars are written below
    }
    auto blob = hermes_->tag->Get(name);
    if (blob.empty()) {
      continue;
    }
 #define put_type_sst(T) \
    if (it.second->m_Type == adios2::helper::GetDataType<T>()) { \
      adios2::core::Variable<T> *v = io->InquireVariable<T>(name); \
      if (v) { \
        writer->Put(*v, reinterpret_cast<const T *>(blob.data()), \
                    adios2::Mode::Sync); \
      } \
      continue; \
    }
    ADIOS2_FOREACH_STDTYPE_1ARG(put_type_sst)
 #undef put_type_sst
  }

  if (m_Comm.Rank() == 0) {
    const int32_t fired = fired_now ? 1 : 0;
    const int32_t fire_step = trigger_fire_step_;
    const double stat = trigger_last_stat_;
    if (auto *v = io->InquireVariable<int32_t>("vigil/trigger_fired")) {
      writer->Put(*v, &fired, adios2::Mode::Sync);
    }
    if (auto *v = io->InquireVariable<double>("vigil/trigger_stat")) {
      writer->Put(*v, &stat, adios2::Mode::Sync);
    }
    if (auto *v = io->InquireVariable<int32_t>("vigil/trigger_fire_step")) {
      writer->Put(*v, &fire_step, adios2::Mode::Sync);
    }
  }

  writer->EndStep();
  auto t1 = std::chrono::high_resolution_clock::now();
  if (m_Comm.Rank() == 0) {
    engine_logger->info(
        "SST flagged step {} shipped in {} us (fired_now={}, window_left={})",
        currentStep,
        std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count(),
        fired_now, trigger_window_remaining_);
  }
}
#endif


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
   // When trigger-gated, SST mirroring is deferred to EndStep (from CTE blobs).
   if (CatalystState && CatalystState->CatalystWriter() && !SstGated_())
   {
     adios2::core::IO *catIO = CatalystState->UseSST() ? CatalystState->SSTIO : CatalystState->InlineIO;
     adios2::core::Variable<T> *catVar = catIO->InquireVariable<T>(variable.m_Name);
     if (catVar)
     {
       if (CatalystState->UseSST()) {
         auto t0 = std::chrono::high_resolution_clock::now();
         CatalystState->CatalystWriter()->Put(*catVar, values);
         auto t1 = std::chrono::high_resolution_clock::now();
         sst_put_time_us_ += static_cast<int64_t>(
             std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count());
       } else {
         CatalystState->CatalystWriter()->Put(*catVar, values);
       }
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
  //client.Mdm_insert(clio::run::PoolQuery::Local(), db_op);

}


template<typename T>
void HermesEngine::DoPutDeferred_(
    const adios2::core::Variable<T> &variable, const T *values) {
  TRACE_FUNC(variable.m_Name, adios2::ToString(variable.m_Count));
  std::string name = variable.m_Name;
  const size_t blob_size = variable.SelectionSize() * sizeof(T);
  #ifdef COEUS_HAVE_CATALYST
   // When trigger-gated, SST mirroring is deferred to EndStep (from CTE blobs).
   if (CatalystState && CatalystState->CatalystWriter() && !SstGated_())
   {
     adios2::core::IO *catIO = CatalystState->UseSST() ? CatalystState->SSTIO : CatalystState->InlineIO;
     adios2::core::Variable<T> *catVar = catIO->InquireVariable<T>(variable.m_Name);
     if (catVar)
     {
       if (CatalystState->UseSST()) {
         auto t0 = std::chrono::high_resolution_clock::now();
         CatalystState->CatalystWriter()->Put(*catVar, values);
         auto t1 = std::chrono::high_resolution_clock::now();
         sst_put_time_us_ += static_cast<int64_t>(
             std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count());
       } else {
         CatalystState->CatalystWriter()->Put(*catVar, values);
       }
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
  //client.Mdm_insert(clio::run::PoolQuery::Local(), db_op);


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
    client.Mdm_insert(clio::run::PoolQuery::Local(), db_op);

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
