/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Distributed under BSD 3-Clause license.                                   *
 * Copyright by the Illinois Institute of Technology.                        *
 * Part of Coeus-adapter add-on operators.                                   *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef COEUS_ADDONS_STEP_BLOB_READER_H_
#define COEUS_ADDONS_STEP_BLOB_READER_H_

#include <cstdint>
#include <string>
#include <vector>
#include <memory>

#include "comms/CTEHermes.h"   // reused, unmodified, from coeus-adapter/include

namespace coeus_ops {

/**
 * StepBlobReader — a *read-mostly* CTE client for the add-on operators.
 *
 * The HermesEngine writes every variable of step N (rank r) into a CTE tag
 * named "step_<N>_rank<r>". This class attaches to the SAME pre-deployed CTE
 * core pool the engine uses and reads those blobs back out-of-band. It never
 * links against or modifies the engine — it only reuses the already-existing
 * CTEHermes / CTETagClient comms layer.
 *
 * Tag naming mirrors HermesEngine::BeginStep exactly:
 *     "step_" + step + "_rank" + rank
 */
class StepBlobReader {
 public:
  StepBlobReader() = default;
  ~StepBlobReader() = default;

  /** Attach to the running CTE core pool (Chimaera runtime must be up). */
  bool Connect();

  /** Open the tag for (step, rank). Must be called before Get/BlobSize. */
  bool OpenStep(int step, int rank);

  /** Read a variable's blob for the currently-open step (empty if absent). */
  std::vector<uint8_t> Get(const std::string &var_name);

  /** Size in bytes of a variable's blob in the open step (0 if absent). */
  size_t BlobSize(const std::string &var_name);

  /** All blob names present in the open step. */
  std::vector<std::string> BlobNames();

  /**
   * Online driving helper: poll until `var_name` for (step, rank) has a
   * non-zero blob, or timeout. Returns true once data is present. Used by the
   * streaming (near-line) driver so it can run concurrently with the app.
   */
  bool WaitForStep(int step, int rank, const std::string &var_name,
                   int timeout_ms, int poll_ms = 25);

  /**
   * Optional write-back: store a derived blob (e.g. "<var>_hash", "dpdt")
   * into the *currently-open* step tag. Additive — the engine never reads it,
   * downstream tools can. Disabled unless the driver opts in.
   */
  bool PutDerived(const std::string &blob_name, size_t size, const void *data);

 private:
  std::unique_ptr<coeus::CTEHermes> hermes_;
  bool connected_ = false;
};

}  // namespace coeus_ops
#endif  // COEUS_ADDONS_STEP_BLOB_READER_H_
