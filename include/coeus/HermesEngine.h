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

#ifndef COEUS_INCLUDE_COEUS_HERMESENGINE_H_
#define COEUS_INCLUDE_COEUS_HERMESENGINE_H_

#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <string>

#include <vector>

#include <adios2.h>
#include <adios2/engine/plugin/PluginEngineInterface.h>
#include "ContainerManager.h"
#include "rankConsensus/rankConsensus.h"
#include "coeus/MetadataSerializer.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "common/SQlite.h"
#include "common/YAMLParser.h"
#include <common/ErrorCodes.h>
#include "common/DbOperation.h"
#include "coeus_mdm/coeus_mdm.h"
#include "common/VariableMetadata.h"
#include <comms/Bucket.h>
#include <comms/Hermes.h>
#include <comms/MPI.h>
#include "common/globalVariable.h"
#include "common/Tracer.h"

namespace coeus {
class HermesEngine : public adios2::plugin::PluginEngineInterface {
 public:
  std::shared_ptr<coeus::IHermes> Hermes;
  std::string uid;
  SQLiteWrapper* db;
  std::string db_file;
  hrun::coeus_mdm::Client client;
  hrun::rankConsensus::Client rank_consensus;
//  FileLock* lock;
//  DbQueueWorker* db_worker;
  int ppn;
  GlobalVariable globalData;
  /** Construct the HermesEngine */
  HermesEngine(adios2::core::IO &io, //NOLINT
               const std::string &name,
               const adios2::Mode mode,
               adios2::helper::Comm comm);

  HermesEngine(std::shared_ptr<coeus::IHermes> h,//NOLINT
               std::shared_ptr<coeus::MPI> mpi,
               adios2::core::IO &io,
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
  bool open = false;

  int currentStep = 0;
  int total_steps = -1;

//  std::shared_ptr<coeus::MPI> mpiComm;
  uint rank;
  int comm_size;

  YAMLMap variableMap;
  YAMLMap operationMap;

  std::vector<std::string> listOfVars;

  std::shared_ptr<spdlog::logger> engine_logger;
  std::shared_ptr<spdlog::logger> meta_logger_put;
  std::shared_ptr<spdlog::logger> meta_logger_get;
  void IncrementCurrentStep();

  template<typename T>
  T* SelectUnion(adios2::PrimitiveStdtypeUnion &u);

  template<typename T>
  void ElementMinMax(adios2::MinMaxStruct &MinMax, void* element);

  void LoadMetadata();

  void DefineVariable(const VariableMetadata& variableMetadata);

 protected:
  /** Initialize (wrapper around Init_)*/
  void Init() override { Init_(); }

  /** Actual engine initialization */
  void Init_();

  /** Close a particular transport */
  void DoClose(const int transportIndex = -1) override;

  /** Place data in Hermes */
  template<typename T>
  void DoPutSync_(const adios2::core::Variable<T> &variable,
                  const T *values);

    /** Place data in Hermes asynchronously */
  template<typename T>
  void DoPutDeferred_(const adios2::core::Variable<T> &variable,
                      const T *values);

  /** Get data from Hermes (sync) */
  template<typename T>
  void DoGetSync_(const adios2::core::Variable<T> &variable,
                  T *values);

  /** Get data from Hermes (async) */
  template<typename T>
  void DoGetDeferred_(const adios2::core::Variable<T> &variable,
                      T *values);

  /** Calls to support Adios native queries */
  void ApplyElementMinMax(adios2::MinMaxStruct &MinMax, adios2::DataType Type,
                                 void *Element);

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

#endif  // COEUS_INCLUDE_COEUS_HERMESENGINE_H_
