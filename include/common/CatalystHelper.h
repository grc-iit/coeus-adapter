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

#ifndef COEUS_INCLUDE_COMMON_CATALYSTHELPER_H_
#define COEUS_INCLUDE_COMMON_CATALYSTHELPER_H_

#ifdef COEUS_HAVE_CATALYST

#include <iostream>
#include <sstream>
#include <string>
// IMPORTANT: Include HermesEngine.h (and thus ADIOS2) BEFORE Catalyst/Conduit
// headers. Conduit defines preprocessor macros that conflict with ADIOS2's
// Variable<T> template class, causing parse errors in adios2/core/Variable.h.
#include "coeus/HermesEngine.h"
#include <catalyst.hpp>
#include <catalyst_conduit.hpp>

namespace coeus {

void HermesEngine::CatalystConfig()
{
  std::cout << "\tCatalyst Library Version: " << CATALYST_VERSION << "\n";
  std::cout << "\tCatalyst ABI Version: " << CATALYST_ABI_VERSION << "\n";
  conduit_cpp::Node node;
  catalyst_about(conduit_cpp::c_node(&node));
  auto implementation = node.has_path("catalyst/implementation")
                            ? node["catalyst/implementation"].as_string()
                            : std::string("stub");
  std::cout << "\tImplementation: " << implementation << "\n\n";
}

void HermesEngine::CatalystInit()
{
  conduit_cpp::Node node;
  node["catalyst/scripts/script/filename"].set(CatalystState->ScriptFileName);

  std::ostringstream address;
  address << &CatalystState->InlineIO;

  node["catalyst/fides/json_file"].set(CatalystState->JSONFileName);
  node["catalyst/fides/data_source_io/source"].set(std::string("source"));
  node["catalyst/fides/data_source_io/address"].set(address.str());
  node["catalyst/fides/data_source_path/source"].set(std::string("source"));
  node["catalyst/fides/data_source_path/path"].set(std::string("DataReader"));
  catalyst_initialize(conduit_cpp::c_node(&node));

  if (rank == 0)
  {
    this->CatalystConfig();
  }
}

void HermesEngine::CatalystExecute()
{
  // Only run in-process Catalyst for inline (single-node); SST uses external reader
  if (!CatalystState || CatalystState->UseSST() || !CatalystState->InlineWriter) {
    if (CatalystState && CatalystState->UseSST())
      return;  // normal: SST mode, reader is external
    engine_logger->warn("CatalystExecute called but InlineWriter is null");
    return;
  }
  
  try {
    // Match reference implementation: use InlineWriter->CurrentStep() if available,
    // otherwise fall back to main engine's currentStep
    int64_t timestep;
    if (inline_writer_in_step_) {
      // InlineWriter supports steps, use its CurrentStep() like the reference
      timestep = static_cast<int64_t>(CatalystState->InlineWriter->CurrentStep());
    } else {
      // InlineWriter doesn't support steps, use main engine's step counter
      timestep = static_cast<int64_t>(currentStep);
    }
    conduit_cpp::Node node;
    node["catalyst/state/timestep"].set(timestep);
    node["catalyst/state/time"].set(timestep);
    node["catalyst/channels/fides/type"].set(std::string("fides"));

    std::ostringstream address;
    address << &CatalystState->InlineIO;

    node["catalyst/fides/json_file"].set(CatalystState->JSONFileName);
    node["catalyst/fides/data_source_io/source"].set(std::string("source"));
    node["catalyst/fides/data_source_io/address"].set(address.str());
    node["catalyst/fides/data_source_path/source"].set(std::string("source"));
    node["catalyst/fides/data_source_path/path"].set(std::string("DataReader"));

    conduit_cpp::Node dummy;
    dummy["dummy"].set(0);
    node["catalyst/channels/fides/data"].set(dummy);

    catalyst_execute(conduit_cpp::c_node(&node));
  } catch (const std::exception& e) {
    engine_logger->error("CatalystExecute failed: {}", e.what());
    throw;
  } catch (...) {
    engine_logger->error("CatalystExecute failed with unknown exception");
    throw;
  }
}

} // namespace coeus

#endif // COEUS_HAVE_CATALYST

#endif // COEUS_INCLUDE_COMMON_CATALYSTHELPER_H_

