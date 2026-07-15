/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Distributed under BSD 3-Clause license.                                   *
 * Copyright by the Illinois Institute of Technology.                        *
 * Part of Coeus-adapter add-on operators.                                   *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "coeus_ops/StepBlobReader.h"

#include <chrono>
#include <thread>

namespace coeus_ops {

static std::string StepTag(int step, int rank) {
  return "step_" + std::to_string(step) + "_rank" + std::to_string(rank);
}

bool StepBlobReader::Connect() {
  if (connected_) return true;
  hermes_ = std::make_unique<coeus::CTEHermes>();
  if (!hermes_->connect()) return false;   // attaches to pre-deployed CTE pool
  connected_ = true;
  return true;
}

bool StepBlobReader::OpenStep(int step, int rank) {
  if (!connected_) return false;
  // GetTag deletes any previous tag handle and (get-or-)creates this one.
  return hermes_->GetTag(StepTag(step, rank));
}

std::vector<uint8_t> StepBlobReader::Get(const std::string &var_name) {
  if (!connected_ || !hermes_->tag) return {};
  return hermes_->tag->Get(var_name);
}

size_t StepBlobReader::BlobSize(const std::string &var_name) {
  if (!connected_ || !hermes_->tag) return 0;
  return hermes_->tag->GetBlobSize(var_name);
}

std::vector<std::string> StepBlobReader::BlobNames() {
  if (!connected_ || !hermes_->tag) return {};
  return hermes_->tag->GetContainedBlobNames();
}

bool StepBlobReader::WaitForStep(int step, int rank, const std::string &var_name,
                                 int timeout_ms, int poll_ms) {
  if (!connected_) return false;
  auto deadline = std::chrono::steady_clock::now() +
                  std::chrono::milliseconds(timeout_ms);
  do {
    if (OpenStep(step, rank) && BlobSize(var_name) > 0) return true;
    std::this_thread::sleep_for(std::chrono::milliseconds(poll_ms));
  } while (std::chrono::steady_clock::now() < deadline);
  return false;
}

bool StepBlobReader::PutDerived(const std::string &blob_name, size_t size,
                                const void *data) {
  if (!connected_ || !hermes_->tag) return false;
  hermes_->tag->Put(blob_name, size, data);
  return true;
}

}  // namespace coeus_ops
