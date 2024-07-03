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

#include "hrun_admin/hrun_admin.h"
#include "hrun/api/hrun_runtime.h"
#include "coeus_mdm/coeus_mdm.h"
#include "hermes/hermes.h"
#include <cstring>

#include "common/SQlite.h"

#include "state_diff.hpp"
#include <Kokkos_Core.hpp>

namespace hrun::coeus_mdm {

class Server : public TaskLib {
private:
  std::unique_ptr<SQLiteWrapper> db;
  hermes::blob_mdm::Client hermes_client;

  float error_tolerance = 1e-4; // Application error tolerance
  int chunk_size = 512; // Target chunk size. This example uses 16 bytes
  bool fuzzy_hash = true; // Set to true to use our rounding hash algorithm. Otherwise, directly hash blocks of FP values
  char dtype = 'f'; // float
  int root_level = 1; // builds the tree from the leaf level to level 1 (root level). For better parallelism, set root_level to 12 or 13.

 public:
  Server() = default;

  void Construct(ConstructTask *task, RunContext &rctx) {
    task->Deserialize();
    db = std::make_unique<SQLiteWrapper>(task->db_path_->str());
    hermes_client.Init(task->hermes_id_, HRUN_ADMIN->queue_id_);
#IFDEF USE_HASH
    Kokkos::initialize();
#ENDIF
    task->SetModuleComplete();
  }

  void Destruct(DestructTask *task, RunContext &rctx) {
    task->SetModuleComplete();
  }

  void Mdm_insert(Mdm_insertTask *task, RunContext &rctx) {
    DbOperation db_op = task->GetDbOp();

    if (db_op.type == OperationType::InsertData) {

      db->InsertVariableMetadata(db_op.step, db_op.rank, db_op.metadata);
      db->InsertBlobLocation(db_op.step, db_op.rank, db_op.name, db_op.blobInfo);

    } else if (db_op.type == OperationType::UpdateSteps) {
      db->UpdateTotalSteps(db_op.uid, db_op.currentStep);

    }

    task->SetModuleComplete();
  }

  void Put_hash(Put_hashTask *task, RunContext &rctx) {
//    float* data_run0_ptr = (float*)data_run0_h.data();
//
//    Kokkos::View<float*> data_run0_d("Run0 Data", data_run0_h.size());
//    Kokkos::View<float*, Kokkos::HostSpace, Kokkos::MemoryTraits<Kokkos::Unmanaged> > data_run0_h_kokkos(data_run0_ptr, data_run0_h.size());
//    Kokkos::deep_copy(data_run0_d, data_run0_h_kokkos);
//    std::cout << "EXEC STATE:: Data loaded and transfered to GPU" << std::endl;
//
//    // Create tree_run0
//    CompareTreeDeduplicator tree_object(chunk_size, root_level, fuzzy_hash, error_tolerance, dtype);
//    tree_object.setup(data_len);
//    tree_object.create_tree((uint8_t*)data_run0_d.data(), data_run0_d.size());
//    std::cout << "EXEC STATE:: Tree created" << std::endl;
//
//    // Serialize tree
//    std::vector<uint8_t> serialized_buffer;
//    serialized_buffer = tree_object.serialize();
//    std::cout << "EXEC STATE:: Tree serialized" << std::endl;

    hshm::charbuf blob_name = hshm::to_charbuf(*task->blob_name_);
    hshm::charbuf good_name(blob_name.str() + "_hash");
    char *blob_buf = HRUN_CLIENT->GetDataPointer(task->data_);
    size_t blob_size = task->data_size_;
    size_t float_size = blob_size/sizeof(float);

    std::vector<float> data_run0_h(float_size);
    std::memcpy(data_run0_h, blob_buf, blob_size);

    float* data_run0_ptr = (float*)data_run0_h.data();
    Kokkos::View<float*> data_run0_d("Run0 Data", data_run0_h.size());
    Kokkos::View<float*, Kokkos::HostSpace, Kokkos::MemoryTraits<Kokkos::Unmanaged> > data_run0_h_kokkos(data_run0_ptr, data_run0_h.size());
    Kokkos::deep_copy(data_run0_d, data_run0_h_kokkos);
    std::cout << "EXEC STATE:: Data loaded" << std::endl;

    // Create tree_run0
    CompareTreeDeduplicator tree_object(chunk_size, root_level, fuzzy_hash, error_tolerance, dtype);
    tree_object.setup(data_len);
    tree_object.create_tree((uint8_t*)data_run0_d.data(), data_run0_d.size());
    std::cout << "EXEC STATE:: Tree created" << std::endl;

    // Serialize tree
    std::vector<uint8_t> serialized_buffer;
    serialized_buffer = tree_object.serialize();
    std::cout << "EXEC STATE:: Tree serialized" << std::endl;

    LPointer<char> p = HRUN_CLIENT->AllocateBufferClient(blob_data.size());
    std::memcpy(p.ptr_, serialized_buffer, serialized_buffer.size());

    hermes_client.AsyncPut(task->task_node_ + 1, task->tag_id_, good_name, taks->blob_id_, task->blob_off_,
                           serialized_buffer.size(), p.ptr_, task->score_, task->flags_);
    task->SetModuleComplete();
  }

 public:
#include "coeus_mdm/coeus_mdm_lib_exec.h"
};

}  // namespace hrun::coeus_mdm

HRUN_TASK_CC(hrun::coeus_mdm::Server, "coeus_mdm");
