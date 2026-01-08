//
// Created by llogan on 28/10/24.
//
#include <hermes_shm/util/logging.h>
#include <hermes_shm/util/random.h>
#include <unistd.h>

#include "basic_test.h"

TEST_CASE("RandomCountDistribution") {
  hshm::CountDistribution x;
  x.Shape(0);
  for (size_t i = 0; i < 10; ++i) {
    REQUIRE(x.GetInt() == i);
  }
}

TEST_CASE("RandomNormalDistribution") {
  hshm::NormalDistribution x;
  x.Shape(10, 10);
  for (size_t i = 0; i < 10; ++i) {
    x.GetInt();
    x.GetDouble();
    x.GetSize();
  }
}

TEST_CASE("RandomGammaDistribution") {
  hshm::GammaDistribution x;
  x.Shape(1, 10);
  for (size_t i = 0; i < 10; ++i) {
    x.GetInt();
    x.GetDouble();
    x.GetSize();
  }
}

TEST_CASE("RandomExponentialDistribution") {
  hshm::ExponentialDistribution x;
  x.Shape(2.23);
  for (size_t i = 0; i < 10; ++i) {
    x.GetInt();
    x.GetDouble();
    x.GetSize();
  }
}

TEST_CASE("RandomUniformDistribution") {
  hshm::UniformDistribution x;
  x.Shape(2.23);
  for (size_t i = 0; i < 10; ++i) {
    x.GetInt();
    x.GetDouble();
    x.GetSize();
  }
}
