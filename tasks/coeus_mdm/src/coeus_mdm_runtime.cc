/**
 * Runtime implementation for coeus_mdm (clio-core port).
 *
 * Contains the server-side task processing logic. The Container virtual
 * dispatch API (Init, Run, SaveTask, LoadTask, NewTask, ...) is in
 * autogen/coeus_mdm_lib_exec.cc.
 */

#include "coeus/coeus_mdm/coeus_mdm_runtime.h"

namespace coeus::coeus_mdm {

//===========================================================================
// Method implementations
//===========================================================================

clio::run::TaskResume Runtime::Create(clio::run::shared_ptr<CreateTask>& task) {
  CLIO_TASK_BODY_BEGIN
  HLOG(kDebug, "coeus_mdm: Executing Create task for pool {}", task->pool_id_);

  // Get CreateParams from task
  CreateParams params = task->GetParams();
  db_path_ = params.db_path_;

  // Initialize SQLite database
  db_ = std::make_unique<SQLiteWrapper>(db_path_);
  if (db_) {
    db_->createTables();
  }

  task->return_code_ = 0;
  HLOG(kDebug, "coeus_mdm: Container created for pool: {} (db_path: {})",
       pool_name_, db_path_);
  CLIO_CO_RETURN;
  CLIO_TASK_BODY_END
}

clio::run::TaskResume Runtime::Mdm_insert(clio::run::shared_ptr<Mdm_insertTask>& task) {
  CLIO_TASK_BODY_BEGIN
  HLOG(kDebug, "coeus_mdm: Executing Mdm_insert task");

  if (!db_) {
    HLOG(kError, "coeus_mdm: Database not initialized");
    task->SetReturnCode(1);
    CLIO_CO_RETURN;
  }

  DbOperation db_op = task->GetDbOp();

  if (db_op.type == OperationType::InsertData) {
    db_->InsertVariableMetadata(db_op.step, db_op.rank, db_op.metadata);
    db_->InsertBlobLocation(db_op.step, db_op.rank, db_op.name, db_op.blobInfo);
  } else if (db_op.type == OperationType::UpdateSteps) {
    db_->UpdateTotalSteps(db_op.uid, db_op.currentStep);
  } else if (db_op.type == OperationType::InsertDerivedData) {
    db_->InsertVariableMetadata(db_op.step, db_op.rank, db_op.metadata);
    db_->InsertBlobLocation(db_op.step, db_op.rank, db_op.name, db_op.blobInfo);
    db_->insertOrUpdateDerivedQuantity(db_op.step, db_op.name, "min",
                                       db_op.blobInfo.blob_name, db_op.blobInfo.tag_name,
                                       db_op.derived_semantics.min_value);
    db_->insertOrUpdateDerivedQuantity(db_op.step, db_op.name, "max",
                                       db_op.blobInfo.blob_name, db_op.blobInfo.tag_name,
                                       db_op.derived_semantics.max_value);
  }

  task->SetReturnCode(0);  // Success
  HLOG(kDebug, "coeus_mdm: Mdm_insert completed successfully");
  CLIO_CO_RETURN;
  CLIO_TASK_BODY_END
}

clio::run::TaskResume Runtime::Destroy(clio::run::shared_ptr<DestroyTask>& task) {
  CLIO_TASK_BODY_BEGIN
  HLOG(kDebug, "coeus_mdm: Executing Destroy task - Pool ID: {}",
       task->target_pool_id_);

  // Clean up database connection
  db_.reset();

  task->return_code_ = 0;
  task->error_message_ = clio::run::priv::string(CLIO_PRIV_ALLOC, "");

  HLOG(kDebug, "coeus_mdm: Container destroyed successfully");
  CLIO_CO_RETURN;
  CLIO_TASK_BODY_END
}

clio::run::u64 Runtime::GetWorkRemaining() const {
  // Metadata operations are fast; no long-running work remaining.
  return 0;
}

}  // namespace coeus::coeus_mdm

// Generate ChiMod entry points (alloc_chimod, get_chimod_name, etc.)
CLIO_CHIMOD_CC(coeus::coeus_mdm::Runtime, "coeus_coeus_mdm")
