//
// Created by lukemartinlogan on 3/28/23.
//

#ifndef COEUS_ADAPTER_INCLUDE_COEUS_COEUS_H_
#define COEUS_ADAPTER_INCLUDE_COEUS_COEUS_H_

#include <adios2.h>
#include "adios2/engine/plugin/PluginEngineInterface.h"
#include <iostream>
#include <fstream>
#include <stdio.h>
#include <stdlib.h>
#include <hermes.h>

namespace coeus {

class HermesEngine : public adios2::plugin::PluginEngineInterface {
 public:
  FILE *fp_;
  /** Construct the HermesEngine */
  HermesEngine(adios2::core::IO &adios,
               const std::string &name,
               const adios2::Mode mode,
               adios2::helper::Comm comm);

  /** Destructor */
  ~HermesEngine() override;

  /**
   * Define the beginning of a step. A step is typically the offset from
   * the beginning of a file. It is measured as a size_t.
   *
   * Logically, a "step" represents a snapshot of the data at a specific time,
   * and can be thought of as a frame in a video or a snapshot of a simulation.
   * */
  adios2::StepStatus BeginStep(adios2::StepMode mode,
                               const float timeoutSeconds = -1.0) override;

  /** Define the end of a step */
  void EndStep() override;

  /**
   * Returns the current step
   * */
  size_t CurrentStep() const override;

  /** Execute all deferred puts */
  void PerformPuts() override;

  /** Execute all deferred gets */
  void PerformGets() override;

 protected:
  /** Initialize (wrapper around Init_)*/
  void Init() override { Init_(); }

  /** Actual engine initialization */
  void Init_();

  /** Place data in Hermes */
  template<typename T>
  void DoPutSync_(adios2::core::Variable<T> &variable,
                  const T *values) {
    std::cout << __func__ << std::endl;
    size_t total_size = variable.SelectionSize() * sizeof(T);
    size_t bytes_written = fwrite(values, sizeof(char), total_size, fp_);
  }


  /** Place data in Hermes asynchronously */
  template<typename T>
  void DoPutDeferred_(adios2::core::Variable<T> &variable,
                      const T *values) {
    std::cout << __func__ << std::endl;
    size_t total_size = variable.SelectionSize() * sizeof(T);
    size_t bytes_written = fwrite(values, sizeof(char), total_size, fp_);
  }

  /** Get data from Hermes (sync) */
  template<typename T>
  void DoGetSync_(adios2::core::Variable<T> &variable, T *values) {

    std::cout << __func__ << std::endl;
    size_t total_size = variable.SelectionSize() * sizeof(T);
    size_t bytes_written = fread(values, sizeof(char), total_size, fp_);
  }

  /** Get data from Hermes (async) */
  template<typename T>
  void DoGetDeferred_(adios2::core::Variable<T> &variable, T *values) {
    std::cout << __func__ << std::endl;
    size_t total_size = variable.SelectionSize() * sizeof(T);
    size_t bytes_written = fread(values, sizeof(char), total_size, fp_);
  }

  /** Close a particular transport */
  void DoClose(const int transportIndex = -1) override;

  /**
   * Declares DoPutSync and DoPutDeferred for a number of predefined types.
   * ADIOS2_FOREACH_STDTYPE_1ARG is a macro which iterates over every
   * known type (e.g., int, double, float, etc).
   * */
#define declare_type(T) \
    void DoPutSync(adios2::core::Variable<T> &variable, \
                   const T *values) override { \
      DoPutSync_(variable, values); \
    } \
    void DoPutDeferred(adios2::core::Variable<T> &variable, \
                       const T *values) override { \
      DoPutDeferred_(variable, values);\
    } \
    void DoGetSync(adios2::core::Variable<T> &variable, \
                   T *values) override { \
      DoGetSync_(variable, values); \
    } \
    void DoGetDeferred(adios2::core::Variable<T> &variable, \
                       T *values) override { \
      DoGetDeferred_(variable, values);\
    }
  ADIOS2_FOREACH_STDTYPE_1ARG(declare_type)
#undef declare_type
};

}  // namespace coeus

#endif  // COEUS_ADAPTER_INCLUDE_COEUS_COEUS_H_
