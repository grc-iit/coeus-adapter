//
// Created by jaime on 10/5/2023.
//

#ifndef COEUS_INCLUDE_COMMON_DBWORKER_H_
#define COEUS_INCLUDE_COMMON_DBWORKER_H_

#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include "common/DbOperation.h"
#include "common/SQlite.h"

class DbQueueWorker {
 private:
  SQLiteWrapper* db;
  FileLock* file_lock;
  std::queue<DbOperation> queue;
  std::thread workerThread;
  std::mutex mtx;
  std::condition_variable cv;
  bool running = true;

  void worker() {
    while (running) {
      std::unique_lock<std::mutex> lock(mtx);
      cv.wait(lock, [this]() { return !queue.empty() || !running; });

      file_lock->lock();
      while (!queue.empty()) {
        auto operation = queue.front();
        queue.pop();
        processOperation(operation);
      }
      file_lock->unlock();
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
  }

  ~DbQueueWorker() {
    {
      std::lock_guard<std::mutex> lock(mtx);
      running = false;
    }
    cv.notify_one();
    workerThread.join();
  }

  void enqueue(const DbOperation& op) {
    {
      std::lock_guard<std::mutex> lock(mtx);
      queue.push(op);
    }
    cv.notify_one();
  }
};


#endif //COEUS_INCLUDE_COMMON_DBWORKER_H_
