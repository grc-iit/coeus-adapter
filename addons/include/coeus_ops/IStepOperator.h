/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Distributed under BSD 3-Clause license.                                   *
 * Copyright by the Illinois Institute of Technology.                        *
 * Part of Coeus-adapter add-on operators.                                   *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef COEUS_ADDONS_ISTEP_OPERATOR_H_
#define COEUS_ADDONS_ISTEP_OPERATOR_H_

#include "coeus_ops/StepBlobReader.h"

namespace coeus_ops {

/**
 * IStepOperator — the add-on operator contract.
 *
 * A driver iterates steps/ranks and calls OnStep() once per (step, rank) with
 * a reader positioned on that step's CTE tag. Operators pull the blobs they
 * need and do their work. This is the ONLY integration surface: the operators
 * know nothing about HermesEngine internals, and the engine knows nothing
 * about the operators.
 */
class IStepOperator {
 public:
  virtual ~IStepOperator() = default;

  /** Called once per (step, rank); reader is already OpenStep()'d on it. */
  virtual void OnStep(int step, int rank, StepBlobReader &reader) = 0;

  /** Called once after the last step (flush summaries, close logs). */
  virtual void Finalize() {}

  /** Human-readable name for logs. */
  virtual const char *Name() const = 0;
};

}  // namespace coeus_ops
#endif  // COEUS_ADDONS_ISTEP_OPERATOR_H_
