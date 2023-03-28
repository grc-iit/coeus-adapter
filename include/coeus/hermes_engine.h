//
// Created by lukemartinlogan on 3/28/23.
//

#ifndef COEUS_ADAPTER_INCLUDE_COEUS_COEUS_H_
#define COEUS_ADAPTER_INCLUDE_COEUS_COEUS_H_

#include <adios2.h>
#include "adios2/engine/plugin/PluginEngineInterface.h"
#include <mpi.h>
#include <iostream>
#include <fstream>
// TODO(llogan): include hermes

namespace coeus {

class HermesEngine : public adios2::plugin::PluginEngineInterface {
 public:
  HermesEngine(adios2::core::IO &adios,
               const std::string &name,
               adios2::helper::Comm
               comm);

  ~HermesEngine() override;

  adios2::StepStatus BeginStep(adios2::StepMode mode,
                               const float timeoutSeconds = -1.0) override;

  void EndStep() override;

  size_t CurrentStep() const override;

  void PerformPuts() override;

 protected:
  template<typename T>
  void _DoPutSync(adios2::core::Variable<T> &variable,
                  const T *values) {
  }

  template<typename T>
  void _DoPutDeferred(adios2::core::Variable<T> &variable,
                      const T *values) {
  }

  void DoClose(const int transportIndex = -1) override;

#define declare_type(T) \
    void DoPutSync(adios2::core::Variable<T> &variable, \
                   const T *values) override { \
      _DoPutSync(variable, values); \
    } \
    void DoPutDeferred(adios2::core::Variable<T> &variable, \
                       const T *values) override { \
      _DoPutDeferred(variable, values);\
    }
  ADIOS2_FOREACH_STDTYPE_1ARG(declare_type)
#undef declare_type

 private:
  struct EngineImpl;
  std::unique_ptr<EngineImpl> Impl;
};

}  // namespace coeus

#endif  // COEUS_ADAPTER_INCLUDE_COEUS_COEUS_H_
