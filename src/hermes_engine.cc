//
// Created by lukemartinlogan on 3/28/23.
//

#include "coeus/hermes_engine.h"

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
  std::cout << __func__ << ": " << io.m_Name << ", " <<  mode << std::endl;
  Init_();
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

void HermesEngine::PerformPuts() {
  std::cout << __func__ << std::endl;
}

void HermesEngine::PerformGets() {
}

/** Close a particular transport */
void HermesEngine::DoClose(const int transportIndex) {
  std::cout << __func__ << std::endl;
  // TODO
}

/**
 * Initialize this engine.
 *
 * How do we know which files to open? The m_IO variable must contain
 * it somewhere. I don't know where yet.
 * */
void HermesEngine::Init_() {
  std::cout << __func__ << std::endl;
  // TODO
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