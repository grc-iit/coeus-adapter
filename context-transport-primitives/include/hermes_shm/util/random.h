/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Distributed under BSD 3-Clause license.                                   *
 * Copyright by The HDF Group.                                               *
 * Copyright by the Illinois Institute of Technology.                        *
 * All rights reserved.                                                      *
 *                                                                           *
 * This file is part of Hermes. The full Hermes copyright notice, including  *
 * terms governing use, modification, and redistribution, is contained in    *
 * the COPYING file, which can be found at the top directory. If you do not  *
 * have access to the file, you may request a copy from help@hdfgroup.org.   *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef HSHM_SHM_INCLUDE_HSHM_SHM_UTIL_RANDOM_H_
#define HSHM_SHM_INCLUDE_HSHM_SHM_UTIL_RANDOM_H_

#include <chrono>
#include <memory>
#include <random>

namespace hshm {

class Distribution {
 protected:
  std::default_random_engine generator;

 public:
  void Seed() {
    generator = std::default_random_engine(
        std::chrono::steady_clock::now().time_since_epoch().count());
  }
  void Seed(size_t seed) { generator = std::default_random_engine(seed); }
  virtual int GetInt() = 0;
  virtual double GetDouble() = 0;
  virtual size_t GetSize() = 0;
};

class CountDistribution : public Distribution {
 private:
  size_t inc_ = 1;
  size_t count_ = 0;

 public:
  void Shape(size_t inc) { inc_ = inc; }
  int GetInt() override {
    int temp = count_;
    count_ += inc_;
    return temp;
  }
  size_t GetSize() override {
    size_t temp = count_;
    count_ += inc_;
    return temp;
  };
  double GetDouble() override {
    double temp = count_;
    count_ += inc_;
    return temp;
  };
};

class NormalDistribution : public Distribution {
 private:
  std::normal_distribution<double> distribution_;

 public:
  NormalDistribution() = default;
  void Shape(double std) {
    distribution_ = std::normal_distribution<double>(0, std);
  }
  void Shape(double mean, double std) {
    distribution_ = std::normal_distribution<double>(mean, std);
  }
  int GetInt() override { return (int)round(GetDouble()); }
  size_t GetSize() override { return (size_t)round(GetDouble()); }
  double GetDouble() override { return distribution_(generator); }
};

class GammaDistribution : public Distribution {
 private:
  std::gamma_distribution<double> distribution_;

 public:
  GammaDistribution() = default;
  void Shape(double scale) {
    distribution_ = std::gamma_distribution<double>(1, scale);
  }
  void Shape(double shape, double scale) {
    distribution_ = std::gamma_distribution<double>(shape, scale);
  }
  int GetInt() override { return (int)round(GetDouble()); }
  size_t GetSize() override { return (size_t)round(GetDouble()); }
  double GetDouble() override { return distribution_(generator); }
};

class ExponentialDistribution : public Distribution {
 private:
  std::exponential_distribution<double> distribution_;

 public:
  ExponentialDistribution() = default;
  void Shape(double scale) {
    distribution_ = std::exponential_distribution<double>(scale);
  }
  int GetInt() override { return (int)round(GetDouble()); }
  size_t GetSize() override { return (size_t)round(GetDouble()); }
  double GetDouble() override { return distribution_(generator); }
};

class UniformDistribution : public Distribution {
 private:
  std::uniform_real_distribution<double> distribution_;

 public:
  UniformDistribution() = default;
  void Shape(size_t high) {
    distribution_ = std::uniform_real_distribution<double>(0, (double)high);
  }
  void Shape(double high) {
    distribution_ = std::uniform_real_distribution<double>(0, high);
  }
  void Shape(double low, double high) {
    distribution_ = std::uniform_real_distribution<double>(low, high);
  }
  int GetInt() override { return (int)round(distribution_(generator)); }
  size_t GetSize() override { return (size_t)round(distribution_(generator)); }
  double GetDouble() override { return distribution_(generator); }
};

}  // namespace hshm

#endif  // HSHM_SHM_INCLUDE_HSHM_SHM_UTIL_RANDOM_H_
