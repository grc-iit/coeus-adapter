//
// Created by lukemartinlogan on 3/28/23.
//

#include "coeus/hermes_engine.h"

namespace coeus {

struct HermesEngine::EngineImpl {
  adios2::core::IO *Io;
  adios2::core::Engine *Writer;

  EngineImpl(adios2::core::ADIOS &io) {
    Io = &io.DeclareIO("InlinePluginIO");
    Io->SetEngine("inline");
    Writer = &Io->Open("write", adios2::Mode::Write);
  }
};


HermesEngine::HermesEngine(adios2::core::IO &io,
                           const std::string &name,
                           adios2::helper::Comm comm)
  : adios2::plugin::PluginEngineInterface(io, name, adios2::Mode::Write,
                                          comm.Duplicate()),
    Impl(new EngineImpl(io.m_ADIOS)) {

}

HermesEngine::~HermesEngine() {
}

adios2::StepStatus HermesEngine::BeginStep(adios2::StepMode mode,
                                           const float timeoutSeconds) {
}

size_t HermesEngine::CurrentStep() const {
}

void HermesEngine::EndStep() {
}

void HermesEngine::PerformPuts() {
}

void HermesEngine::DoClose(const int transportIndex) {
}

}  // namespace coeus