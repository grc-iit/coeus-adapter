/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Distributed under BSD 3-Clause license.                                   *
 * Copyright by the Illinois Institute of Technology.                        *
 * Part of Coeus-adapter add-on operators.                                   *
 *                                                                           *
 * TimeDerivativeOperator — 4th-order centered time derivatives (dp/dt,      *
 * d2p/dt2) over a 5-step sliding window. Port of the time_derivatives       *
 * branch EmitTimeDerivatives() as a standalone CTE consumer, decoupled from *
 * the engine step loop. Diagnostic by default; can also Put dpdt/d2pdt2     *
 * back into CTE.                                                             *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef COEUS_ADDONS_TDERIV_OPERATOR_H_
#define COEUS_ADDONS_TDERIV_OPERATOR_H_

#include <cstdint>
#include <deque>
#include <map>
#include <string>
#include <vector>

#include "coeus_ops/IStepOperator.h"

namespace coeus_ops {

struct TDerivConfig {
  std::string var_name = "pp";           // source pressure-like variable
  std::string out_dpdt = "dpdt";         // 1st-derivative output name
  std::string out_d2pdt2 = "d2pdt2";     // 2nd-derivative output name
  double dt = 1.0;                        // snapshot spacing (ioutput * dt_sim)
  char dtype = 'f';                       // 'f' float / 'd' double
  bool center_tag = true;                 // emit under window-center step
  bool write_back = false;                // Put derivative blobs into CTE
  std::string timing_csv;                 // optional per-emit CSV path
};

/**
 * Maintains a 5-snapshot window per rank; once full, applies the classic
 * 4th-order centered stencils and reports per-rank min/max/L2. The stencils
 * are byte-for-byte the ones from the time_derivatives branch:
 *   dp/dt   = (-v4 + 8 v3 - 8 v1 + v0) / (12 dt)
 *   d2p/dt2 = (-v4 + 16 v3 - 30 v2 + 16 v1 - v0) / (12 dt^2)
 */
class TimeDerivativeOperator : public IStepOperator {
 public:
  explicit TimeDerivativeOperator(TDerivConfig cfg);
  ~TimeDerivativeOperator() override;
  void OnStep(int step, int rank, StepBlobReader &reader) override;
  void Finalize() override;
  const char *Name() const override { return "tderiv"; }

 private:
  struct Window {
    std::deque<int> steps;                       // up to 5 recent step indices
    std::deque<std::vector<uint8_t>> buffers;    // parallel snapshot copies
  };
  void Emit(int center_step, int rank, StepBlobReader &reader,
            const Window &w);

  TDerivConfig cfg_;
  std::map<int, Window> per_rank_;   // rank -> sliding window
  int emit_count_ = 0;
};

}  // namespace coeus_ops
#endif  // COEUS_ADDONS_TDERIV_OPERATOR_H_
