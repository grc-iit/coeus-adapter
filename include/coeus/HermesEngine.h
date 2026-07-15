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
#include <map>
#include <random>
#include <adios2.h>
#include <adios2/engine/plugin/PluginEngineInterface.h>
#include <adios2/core/VariableDerived.h>
#include "adios2/helper/adiosType.h"
#include "ContainerManager.h"
#include <clio_runtime/clio_runtime.h>
#include <clio_runtime/admin/admin_client.h>
#include "coeus/coeus_mdm/coeus_mdm_client.h"
#include "coeus/rankConsensus/rankConsensus_client.h"
#include "coeus/MetadataSerializer.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "common/SQlite.h"
#include "common/YAMLParser.h"
#include <common/ErrorCodes.h>
#include "common/DbOperation.h"
#include "common/VariableMetadata.h"
#include <comms/interfaces/IHermes.h>
#include <comms/MPI.h>
#include <clio_cte/core/core_tasks.h>
#include "common/globalVariable.h"
#include "common/Tracer.h"
#include <clio_cte/core/core_client.h>
#include <memory>


#ifdef COEUS_HAVE_CATALYST
#include <catalyst.hpp>
// conduit comes with Catalyst 2
#include <catalyst_conduit.hpp>

#endif
namespace coeus {

class HermesEngine : public adios2::plugin::PluginEngineInterface {
 public:
  coeus::IHermes* hermes_ = nullptr;  // CTE Hermes interface for blob operations
  std::string uid;
  SQLiteWrapper* db = nullptr;
  std::string db_file;
  std::string adiosOutput;
  int lookahead;
  int index = 0;
  coeus::coeus_mdm::Client client;
  int num_layers = 4;
  int ppn;
  int limit = 0;
  coeus::rankConsensus::Client rank_consensus;
  clio::run::PoolId coeus_mdm_pool_id_;
  clio::run::PoolId rankConsensus_pool_id_;
//  FileLock* lock;
//  DbQueueWorker* db_worker;
  GlobalVariable globalData;
  /** Construct the HermesEngine */
  HermesEngine(adios2::core::IO &io, //NOLINT
               const std::string &name,
               const adios2::Mode mode,
               adios2::helper::Comm comm);

  // Test constructor - no longer needs IHermes abstraction
  HermesEngine(std::shared_ptr<coeus::MPI> mpi,
               adios2::core::IO &io,
               const std::string &name,
               const adios2::Mode mode,
               adios2::helper::Comm comm);

  /** Destructor */
  ~HermesEngine() override;
  #ifdef COEUS_HAVE_CATALYST
  // In-situ Catalyst/Fides integration state
  // Single-node: InlineIO/InlineWriter (Catalyst reads in-process).
  // Multi-node: SSTIO/SSTWriter (Catalyst connects as external SST reader).
  struct CatalystImpl {
    adios2::core::IO *InlineIO = nullptr;
    adios2::core::Engine *InlineWriter = nullptr;
    adios2::core::IO *SSTIO = nullptr;
    adios2::core::Engine *SSTWriter = nullptr;
    std::string ScriptFileName;
    std::string JSONFileName;
    std::string CatalystStreamName;  // If non-empty, use SST for multi-node
    bool UseSST() const { return SSTWriter != nullptr; }
    adios2::core::Engine *CatalystWriter() const {
      return SSTWriter ? SSTWriter : InlineWriter;
    }
  };

#endif

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
//  int reader_get_time = 0;
//  int compare_time = 0;
//  int inintial_time = 0;
//  int begin_step_time = 0 ;
//  int compute_derived_time = 0;
//  int put_time = 0;
#ifdef COEUS_HAVE_CATALYST
// Catalyst helpers
void CatalystConfig();
void CatalystInit();
void CatalystExecute();
  std::unique_ptr<CatalystImpl> CatalystState;
  bool inline_writer_in_step_ = false;  // Track if InlineWriter BeginStep was called
  /** Accumulated time (microseconds) spent in SST Put calls during current step (in-transit). */
  int64_t sst_put_time_us_ = 0;
  /** Ship the current step's fields (from CTE blobs) plus trigger state over SST. */
  void StreamFlaggedStepToSST_(bool fired_now);
#endif

  // --- Statistical trigger (Vigil trigger-render-reason: trigger phase) ---
  // Configured via ADIOS2 XML engine parameters:
  //   TriggerVariable      variable whose global variance is monitored. Either a
  //                        raw field (e.g. "V") or an ADIOS2 derived-quantity
  //                        variance (e.g. "derive/VarV" from "variance(x)") —
  //                        derived is detected automatically and pooled exactly
  //                        across blocks (never averaged).
  //   TriggerSumVariable   optional derived block-sum (e.g. "derive/AddV" from
  //                        "add(x)") supplying per-block means for the pooled
  //                        combine; without it the block sum is taken from the
  //                        raw source field's CTE blob.
  //   TriggerThreshold     fire when variance >= this absolute value
  //   TriggerBaselineRatio fire when variance >= ratio * first-step variance
  //   TriggerInspectSteps  steps streamed per fire, incl. the firing step (default 3)
  //   TriggerRefire        "true" to allow firing on later rising edges (default once)
  //   TriggerLogFile       JSONL fire log written by rank 0 (default trigger_log.jsonl)
  bool trigger_enabled_ = false;
  std::string trigger_variable_;
  std::string trigger_sum_variable_;

  // --- Dissipation trigger (Xcompact3d TGV: two-stage Yellow/Red escalation) ---
  // Selected with TriggerType=dissipation. Inputs are two derived block-mean
  // variables (custom ADIOS2 "mean" op) pooled N_b-weighted across ranks:
  //   TriggerKEVariable         block-mean TKE, e.g. "tke_mean" = mean(0.5|u|^2)
  //   TriggerEnstrophyVariable  block-mean enstrophy, e.g. "enst_mean"
  //   TriggerNu                 physical kinematic viscosity (1/Re)
  //   TriggerOutputDt           simulation time between output steps (dt*ioutput)
  // Metrics per step:  eps_total = -dE_k/dt   (time-differenced global TKE)
  //                    eps_phys  = 2*nu*<enstrophy>
  //                    eps_frac  = (eps_total - eps_phys)/eps_total   [eps_num share]
  //                    nu_ratio  = eps_total/eps_phys                 [nu_eff/nu]
  // Yellow (log-only escalation) and Red (fire: opens the SST inspect window):
  //   TriggerYellowFraction / TriggerRedFraction   eps_frac thresholds (0.05 / 0.15)
  //   TriggerYellowNuRatio  / TriggerRedNuRatio    nu_ratio thresholds (1.05 / 1.2)
  //   TriggerMetricsLogFile  optional JSONL of every step's metrics (rank 0)
  // TriggerType selects the statistic/scheme: "variance" (default) pools
  // per-block variance of TriggerVariable; "mean" pools an ADIOS2 derived
  // per-block MEAN N_b-weighted into the exact global mean (e.g.
  // derive/V2mean = mean(|v|^2) = 3*T* for the LAMMPS temperature trigger),
  // sharing the threshold/baseline/inspect-window path; "dissipation" is the
  // two-stage Yellow/Red numerical-dissipation scheme below.
  std::string trigger_type_ = "variance";
  std::string trigger_ke_variable_;
  std::string trigger_enst_variable_;
  double trigger_nu_ = 0.0;
  double trigger_output_dt_ = 0.0;
  double trigger_yellow_fraction_ = 0.05;
  double trigger_red_fraction_ = 0.15;
  double trigger_yellow_nu_ratio_ = 1.05;
  double trigger_red_nu_ratio_ = 1.2;
  std::string trigger_metrics_log_file_;
  double trigger_prev_ke_ = -1.0;      // global TKE at previous step (< 0 = none yet)
  bool trigger_yellow_prev_ = false;   // for Yellow rising-edge detection
  double trigger_threshold_ = 0.0;       // <= 0 disables the absolute test
  double trigger_baseline_ratio_ = 0.0;  // <= 0 disables the ratio test
  int trigger_inspect_steps_ = 3;
  bool trigger_refire_ = false;
  std::string trigger_log_file_ = "trigger_log.jsonl";
  // Runtime state
  double trigger_baseline_ = -1.0;       // variance at first evaluated step
  double trigger_last_stat_ = 0.0;       // most recent global variance
  bool trigger_prev_condition_ = false;  // for rising-edge detection
  bool trigger_has_fired_ = false;
  int trigger_fire_step_ = -1;
  int trigger_window_remaining_ = 0;     // > 0 while an inspect window is open

  // --- Collapse WARNING mode ("expanding to blank"), variance/mean triggers ---
  // With TriggerWarnOnCollapse=true the trigger no longer fires on the rise;
  // instead it ARMS when the statistic rises above TriggerBaselineRatio*baseline
  // (the structure formed / peaked) and then WARNS on the first step where the
  // statistic falls back to <= TriggerCollapseThreshold or
  // <= TriggerCollapseBaselineRatio*baseline — the field is homogenising toward
  // a uniform/blank state. The warning opens the SST inspect window (streams the
  // firing step + TriggerInspectSteps-1 more to the agent) and logs a WARNING;
  // it does NOT stop the run. The AI agent inspects the streamed steps and
  // issues the fire verdict (e.g. writes the halt flag the simulation polls).
  // On the saturating Gray-Scott (F=0.08,k=0.03) variance peaks ~30x then
  // collapses; arm=20x, collapse=13x warns at output ~23 (streams 23-26). The
  // spots regime never arms (variance peaks ~1.2x), so it never warns.
  bool trigger_warn_on_collapse_ = false;
  double trigger_collapse_threshold_ = 0.0;       // <= 0 disables the absolute test
  double trigger_collapse_baseline_ratio_ = 0.0;  // <= 0 disables the ratio test
  bool trigger_armed_ = false;                    // statistic rose above the arm level
  bool trigger_warned_ = false;                   // collapse warning already fired

  /** Evaluate the trigger for the current step (collective). Returns true on a new fire/warning. */
  bool EvaluateTrigger_();
  /** Collapse warning: arm on the rise, warn (stream to agent) on the collapse (collective). */
  bool EvaluateCollapseWarning_(double stat, const char *stat_name);
  /** Exact pooled global variance of `name` over all ranks (collective; NaN if unavailable). */
  double ComputeGlobalVariance_(const std::string &name);
  /** Pooled global variance from a derived per-block variance (collective; NaN if unavailable). */
  double ComputeGlobalVarianceDerived_(adios2::core::VariableDerived *derivedVar);
  /** Two-stage Yellow/Red dissipation trigger (collective). Returns true on Red fire. */
  bool EvaluateDissipationTrigger_();
  /** N_b-weighted global mean from a derived per-block mean (collective; NaN if unavailable). */
  double ComputeGlobalBlockMean_(const std::string &name);
  /** Sum of all elements in a CTE blob interpreted as double/float; count returned via n. */
  bool SumBlob_(const std::string &name, double &sum, double &n);
  /** True when SST field mirroring is deferred to EndStep and gated on the trigger. */
  bool SstGated_() const;
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
//adios.defineVariable()
//adios.put(variable) -> PutDefereed()
//adios.put( variable, "U+V") -> CalculateDerivedQuantity() -> Put(varaible)
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

  void ComputeDerivedVariables();

  template<typename T>
  DbOperation generateMetadata(adios2::core::Variable<T> variable);
  DbOperation generateMetadata(adios2::core::VariableDerived variable,
                               float *values, int total_count);

  template<typename T>
  void PutDerived(adios2::core::VariableDerived variable, T *values);

  /** Get data from Hermes (sync) */
  template<typename T>
  void DoGetSync_(const adios2::core::Variable<T> &variable,
                  T *values);
//  void HermesEngine::DoGetDerivedVariableSync_(const adios2::core::Variable<T> &variable,
//                                                     T *values);
  /** Get data from Hermes (async) */
  template<typename T>
  void DoGetDeferred_(const adios2::core::Variable<T> &variable,
                      T *values);
//  void DoGetDerivedVariableDeferred_(const adios2::core::Variable<T> &variable,
//                            T *values);
  /** Calls to support Adios native queries */
  void ApplyElementMinMax(adios2::MinMaxStruct &MinMax, adios2::DataType Type,
                                 void *Element);

    bool Demote(int step);
    bool Promote(int step);
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
