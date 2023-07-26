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

/*#ifndef COEUS_ADAPTER_INCLUDE_COEUS_COEUS_H_
#define COEUS_ADAPTER_INCLUDE_COEUS_COEUS_H_*/

#ifndef INCLUDE_COEUS_HERMES_ENGINE_H_
#define INCLUDE_COEUS_HERMES_ENGINE_H_
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <string>

#include <vector>

#include <hermes.h>
#include <adios2.h>
#include <adios2/engine/plugin/PluginEngineInterface.h>



#include "coeus/metadata_serializer.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"

namespace coeus {
class HermesEngine : public adios2::plugin::PluginEngineInterface {
 public:
  /** Construct the HermesEngine */
  HermesEngine(adios2::core::IO &adios, //NOLINT
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
  size_t CurrentStep() const final;

  /** Execute all deferred puts */
  void PerformPuts() override {engine_logger->info("rank {}", rank);}

  /** Execute all deferred gets */
  void PerformGets() override {engine_logger->info("rank {}", rank);}

  bool VariableMinMax(const adios2::core::VariableBase &, const size_t Step,
                        adios2::MinMaxStruct &MinMax) override;

 private:
  int currentStep = 0;

  int rank;
  int comm_size;

  std::vector<std::string> listOfVars;

  std::shared_ptr<spdlog::logger> engine_logger;

  void IncrementCurrentStep();

  void LoadExistingVariables();

  void LoadMetadata();

  void DefineVariable(VariableMetadata variableMetadata);

  template<typename T>
  void HermesPut(const std::string &bucket_name,
                 const std::string &blob_name, size_t blob_size, T values);

  hermes::Blob HermesGet(const std::string &bucket_name,
                         const std::string &varName);

 protected:
  /** Initialize (wrapper around Init_)*/
  void Init() override { Init_(); }

  /** Actual engine initialization */
  void Init_();

  /** Place data in Hermes */
  template<typename T>
  void DoPutSync_(const adios2::core::Variable<T> &variable,
                  const T *values) {engine_logger->info("rank {}", rank);}

  static void ApplyElementMinMax(adios2::MinMaxStruct &MinMax, adios2::DataType Type,
                                   void *Element);

  static void InitElementMinMax(adios2::MinMaxStruct &MinMax,
                                         adios2::DataType Type);


    /** Place data in Hermes asynchronously */
  template<typename T>
  void DoPutDeferred_(const adios2::core::Variable<T> &variable,
                      const T *values);

  /** Get data from Hermes (sync) */
  template<typename T>
  void DoGetSync_(const adios2::core::Variable<T> &variable,
                  T *values) {engine_logger->info("rank {}", rank);}

  /** Get data from Hermes (async) */
  template<typename T>
  void DoGetDeferred_(const adios2::core::Variable<T> &variable,
                      T *values);

  /** Close a particular transport */
  void DoClose(const int transportIndex = -1) override {
      engine_logger->info("rank {}", rank);}

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

#endif  // INCLUDE_COEUS_HERMES_ENGINE_H_
