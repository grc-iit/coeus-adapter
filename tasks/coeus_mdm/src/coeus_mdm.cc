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

#include "common/SQlite.h"

namespace hrun::coeus_mdm {

class Server : public TaskLib {
private:
  std::unique_ptr<SQLiteWrapper> db;
 public:
  Server() = default;

  void Construct(ConstructTask *task, RunContext &rctx) {
    task->Deserialize();
    db = std::make_unique<SQLiteWrapper>(task->db_path_->str());
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

 public:
#include "coeus_mdm/coeus_mdm_lib_exec.h"
};

}  // namespace hrun::coeus_mdm

HRUN_TASK_CC(hrun::coeus_mdm::Server, "coeus_mdm");
