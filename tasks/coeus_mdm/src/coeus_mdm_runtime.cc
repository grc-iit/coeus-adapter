/**
 * Runtime implementation for coeus_mdm
 *
 * Contains the server-side task processing logic.
 */

#include "../include/chimaera/coeus_mdm/coeus_mdm_runtime.h"

namespace chimaera::coeus_mdm {

// Virtual method implementations (Init, Run, DelTask, SaveTask, LoadTask, NewCopy, Aggregate) 
// are in autogen/coeus_mdm_lib_exec.cc

//===========================================================================
// Method implementations
//===========================================================================

void Runtime::Create(hipc::FullPtr<CreateTask> task, chi::RunContext& rctx) {
  HLOG(kDebug, "coeus_mdm: Executing Create task for pool {}", task->pool_id_);

  // Get CreateParams from task
  auto params = task->GetParams(CHI_IPC->GetMainAlloc());
  db_path_ = params.db_path_;

  // Initialize SQLite database
  db_ = std::make_unique<SQLiteWrapper>(db_path_);

  // Create tables if needed (only on first rank per node)
  // Note: This logic might need to be adjusted based on your specific requirements
  // For now, we'll create tables on container creation
  if (db_) {
    db_->createTables();
  }

  HLOG(kDebug, "coeus_mdm: Container created and initialized for pool: {} (ID: {}, db_path: {})",
        pool_name_, task->pool_id_, db_path_);
}

void Runtime::Mdm_insert(hipc::FullPtr<Mdm_insertTask> task, chi::RunContext& rctx) {
  HLOG(kDebug, "coeus_mdm: Executing Mdm_insert task");

  if (!db_) {
    HLOG(kError, "coeus_mdm: Database not initialized");
    task->SetReturnCode(1);
    return;
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
                                       db_op.blobInfo.blob_name, db_op.blobInfo.bucket_name,
                                       db_op.derived_semantics.min_value);
    db_->insertOrUpdateDerivedQuantity(db_op.step, db_op.name, "max",
                                       db_op.blobInfo.blob_name, db_op.blobInfo.bucket_name,
                                       db_op.derived_semantics.max_value);
  }

  task->SetReturnCode(0);  // Success
  HLOG(kDebug, "coeus_mdm: Mdm_insert completed successfully");
}

void Runtime::Destroy(hipc::FullPtr<DestroyTask> task, chi::RunContext& rctx) {
  HLOG(kDebug, "coeus_mdm: Executing Destroy task - Pool ID: {}",
        task->target_pool_id_);

  // Clean up database connection
  db_.reset();

  // Initialize output values
  task->return_code_ = 0;
  task->error_message_ = "";

  HLOG(kDebug, "coeus_mdm: Container destroyed successfully");
}

chi::u64 Runtime::GetWorkRemaining() const {
  // Return 0 as metadata operations are typically fast
  return 0;
}

}  // namespace chimaera::coeus_mdm

