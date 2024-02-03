//
// Created by jaime on 10/5/2023.
//

#ifndef COEUS_INCLUDE_COMMON_DBWORKER_H_
#define COEUS_INCLUDE_COMMON_DBWORKER_H_

#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <labstor/network/local_serialize.h>
#include "common/DbOperation.h"
#include "common/SQlite.h"
#include "hermes_shm/data_structures/ipc/mpsc_ptr_queue.h"

class DbQueueWorker {
 private:
  SQLiteWrapper* db;
  FileLock* file_lock;
  std::thread workerThread;
  bool running = true;
  hipc::uptr<hipc::mpsc_ptr_queue<DbOperation>> queue_;

  void worker() {
    while (running) {
      DbOperation operation;
      while (!queue_->pop(operation).IsNull()) {
        file_lock->lock();
        processOperation(operation);
        file_lock->unlock();
      }
      usleep(20);
    }
  }

  void processOperation(const DbOperation& op) {
    if (op.type == OperationType::InsertData) {
      db->InsertVariableMetadata(op.step, op.rank, op.metadata);
      db->InsertBlobLocation(op.step, op.rank, op.name, op.blobInfo);
    } else if (op.type == OperationType::UpdateSteps) {
      db->UpdateTotalSteps(op.uid, op.currentStep);
    }
  }

 public:
  DbQueueWorker(SQLiteWrapper* _db, FileLock* _lock) : db(_db), file_lock(_lock) {
    workerThread = std::thread(&DbQueueWorker::worker, this);
    queue_ = hipc::make_uptr<hipc::mpsc_queue<DbOperation>>(LABSTOR_CLIENT->main_alloc_);
  }

  ~DbQueueWorker() {
    running = false;
    workerThread.join();
  }

  void enqueue(const DbOperation& op) {
    queue_->emplace(op);
  }
};


#endif //COEUS_INCLUDE_COMMON_DBWORKER_H_
