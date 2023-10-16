//
// Created by lukemartinlogan on 6/29/23.
//

#include "labstor_admin/labstor_admin.h"
#include "labstor/api/labstor_runtime.h"
#include "coeus_mdm/coeus_mdm.h"

namespace labstor::coeus_mdm {

class Server : public TaskLib {
 public:
  Server() = default;

  void Construct(ConstructTask *task, RunContext &rctx) {
    task->SetModuleComplete();
  }

  void Destruct(DestructTask *task, RunContext &rctx) {
    task->SetModuleComplete();
  }

  void SQLInsert(SQLInsertTask *task, RunContext &rctx) {
    task->SetModuleComplete();
  }

 public:
#include "coeus_mdm/coeus_mdm_lib_exec.h"
};

}  // namespace labstor::coeus_mdm

LABSTOR_TASK_CC(labstor::coeus_mdm::Server, "coeus_mdm");
