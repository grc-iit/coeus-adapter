/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Distributed under BSD 3-Clause license.                                   *
 * Part of Coeus-adapter add-on operators.                                   *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "TimeDerivativeOperator.h"

#include <cmath>
#include <cstring>
#include <iostream>

namespace coeus_ops {

TimeDerivativeOperator::TimeDerivativeOperator(TDerivConfig cfg)
    : cfg_(std::move(cfg)) {}

TimeDerivativeOperator::~TimeDerivativeOperator() = default;

void TimeDerivativeOperator::OnStep(int step, int rank, StepBlobReader &reader) {
  std::vector<uint8_t> blob = reader.Get(cfg_.var_name);
  if (blob.empty()) return;

  Window &w = per_rank_[rank];
  w.steps.push_back(step);
  w.buffers.push_back(std::move(blob));
  if (w.steps.size() > 5) {
    w.steps.pop_front();
    w.buffers.pop_front();
  }
  if (w.steps.size() == 5) {
    // center step is the middle of the 5-window (index 2)
    Emit(w.steps[2], rank, reader, w);
  }
}

// Apply the 4th-order centered stencils element-wise; track min/max/L2.
// Takes the 5 snapshot buffers directly (not the private nested Window) so it
// can live at namespace scope.
template <typename T>
static void ApplyStencil(const std::deque<std::vector<uint8_t>> &bufs,
                         double dt, size_t &n_out,
                         double &d1_min, double &d1_max, double &d1_sq,
                         double &d2_min, double &d2_max, double &d2_sq,
                         std::vector<uint8_t> *d1_out,
                         std::vector<uint8_t> *d2_out);

void TimeDerivativeOperator::Emit(int center_step, int rank,
                                  StepBlobReader &reader, const Window &w) {
  const double dt = cfg_.dt;
  size_t n = 0;
  double d1_min = 0, d1_max = 0, d1_sq = 0, d2_min = 0, d2_max = 0, d2_sq = 0;
  std::vector<uint8_t> d1_buf, d2_buf;
  std::vector<uint8_t> *d1p = cfg_.write_back ? &d1_buf : nullptr;
  std::vector<uint8_t> *d2p = cfg_.write_back ? &d2_buf : nullptr;

  if (cfg_.dtype == 'd')
    ApplyStencil<double>(w.buffers, dt, n, d1_min, d1_max, d1_sq, d2_min, d2_max,
                         d2_sq, d1p, d2p);
  else
    ApplyStencil<float>(w.buffers, dt, n, d1_min, d1_max, d1_sq, d2_min, d2_max,
                        d2_sq, d1p, d2p);

  const double d1_l2 = n ? std::sqrt(d1_sq) : 0.0;
  const double d2_l2 = n ? std::sqrt(d2_sq) : 0.0;
  std::cout << "[tderiv] center_step " << center_step << " rank " << rank
            << " var " << cfg_.var_name << " n=" << n << " dt=" << dt
            << " | dpdt[min=" << d1_min << " max=" << d1_max << " l2=" << d1_l2
            << "] d2pdt2[min=" << d2_min << " max=" << d2_max << " l2=" << d2_l2
            << "]\n";

  if (cfg_.write_back) {
    // Emit under the center-of-window step's tag (or the newest step).
    int tag_step = cfg_.center_tag ? center_step : w.steps.back();
    if (reader.OpenStep(tag_step, rank)) {
      reader.PutDerived(cfg_.out_dpdt, d1_buf.size(), d1_buf.data());
      reader.PutDerived(cfg_.out_d2pdt2, d2_buf.size(), d2_buf.data());
    }
    // Re-open the newest step so the driver's position is unchanged.
    reader.OpenStep(w.steps.back(), rank);
  }
  emit_count_++;
}

template <typename T>
static void ApplyStencil(const std::deque<std::vector<uint8_t>> &bufs,
                         double dt, size_t &n_out,
                         double &d1_min, double &d1_max, double &d1_sq,
                         double &d2_min, double &d2_max, double &d2_sq,
                         std::vector<uint8_t> *d1_out,
                         std::vector<uint8_t> *d2_out) {
  const T *p0 = reinterpret_cast<const T *>(bufs[0].data());
  const T *p1 = reinterpret_cast<const T *>(bufs[1].data());
  const T *p2 = reinterpret_cast<const T *>(bufs[2].data());
  const T *p3 = reinterpret_cast<const T *>(bufs[3].data());
  const T *p4 = reinterpret_cast<const T *>(bufs[4].data());
  const size_t n = bufs[2].size() / sizeof(T);
  const double inv12dt = 1.0 / (12.0 * dt);
  const double inv12dt2 = 1.0 / (12.0 * dt * dt);
  if (d1_out) d1_out->resize(n * sizeof(T));
  if (d2_out) d2_out->resize(n * sizeof(T));
  T *o1 = d1_out ? reinterpret_cast<T *>(d1_out->data()) : nullptr;
  T *o2 = d2_out ? reinterpret_cast<T *>(d2_out->data()) : nullptr;

  for (size_t i = 0; i < n; ++i) {
    const double v0 = static_cast<double>(p0[i]);
    const double v1 = static_cast<double>(p1[i]);
    const double v2 = static_cast<double>(p2[i]);
    const double v3 = static_cast<double>(p3[i]);
    const double v4 = static_cast<double>(p4[i]);
    const double d1v = (-v4 + 8.0 * v3 - 8.0 * v1 + v0) * inv12dt;
    const double d2v =
        (-v4 + 16.0 * v3 - 30.0 * v2 + 16.0 * v1 - v0) * inv12dt2;
    if (i == 0) {
      d1_min = d1_max = d1v;
      d2_min = d2_max = d2v;
      d1_sq = d1v * d1v;
      d2_sq = d2v * d2v;
    } else {
      if (d1v < d1_min) d1_min = d1v;
      if (d1v > d1_max) d1_max = d1v;
      if (d2v < d2_min) d2_min = d2v;
      if (d2v > d2_max) d2_max = d2v;
      d1_sq += d1v * d1v;
      d2_sq += d2v * d2v;
    }
    if (o1) o1[i] = static_cast<T>(d1v);
    if (o2) o2[i] = static_cast<T>(d2v);
  }
  n_out = n;
}

void TimeDerivativeOperator::Finalize() {
  std::cout << "[tderiv] emitted " << emit_count_ << " derivative windows\n";
}

}  // namespace coeus_ops
