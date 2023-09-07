/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Distributed under BSD 3-Clause license.                                   *
 * Copyright by the Illinois Institute of Technology.                        *
 * All rights reserved.                                                      *
 *                                                                           *
 * This file is part of Coeus-adapter. The full Coeus-adapter copyright      *
 * notice, including terms governing use, modification, and redistribution,  *
 * is contained in the COPYING file, which can be found at the top directory.*
 * If you do not have access to the file, you may request a copy             *
 * from scslab@iit.edu.                                                      *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef COEUS_INCLUDE_COMMON_THREADPOOL_H_
#define COEUS_INCLUDE_COMMON_THREADPOOL_H_

#include <list>
#include <vector>
#include <thread>
#include <future>
#include <mutex>
#include <functional>

namespace coeus {
template<class T> class Thread;
template<class T> class ThreadPool;

template<class T>
class Thread {
 private:
  ThreadPool<T> *pool_;
  int tid_;
  std::promise<void> loop_cond_;
  std::future<void> thread_complete_;
  std::shared_ptr<T> obj_;
 public:
  Thread() = default;
  Thread(ThreadPool<T> *pool, int tid) : pool_(pool), tid_(tid) {
    obj_ = std::make_shared<T>();
  }

  template<typename ...Args>
  void Assign(Args ...args) {
    loop_cond_ = std::promise<void>();
    thread_complete_ = std::async([this, args...]() { this->Run(args...); });
  }

  template<typename ...Args>
  void Run(Args ...args) {
    try {
      obj_->Run(loop_cond_.get_future(), args...);
    }
    catch(...) {
      throw 1; //TODO: Re-throw exception
    }
  }

  bool IsActive(int timeout_ms = 0) {
    return
        thread_complete_.valid() &&
            thread_complete_.wait_for(std::chrono::milliseconds(timeout_ms))==std::future_status::timeout;
  }

  void Stop() {
    if(thread_complete_.valid());
    loop_cond_.set_value();
  }

  void Wait(int timeout_ms = 25) {
    while(IsActive(timeout_ms));
  }

  int GetTid() {
    return tid_;
  }

  std::shared_ptr<T> GetObj() {
    return obj_;
  }
};

template<class T>
class ThreadPool {
 private:
  uint32_t size_ = 0;
  std::vector<Thread<T>> pool_;
  std::list<Thread<T>*> free_list_;
  std::mutex lock;
 public:
  ThreadPool() = default;
  void Init(uint32_t count)  {
    pool_.reserve(count);
    for(int i = 0; i < count; ++i) {
      pool_.emplace_back(this, i);
      free_list_.emplace_back(&pool_[i]);
    }
  }

  template<typename ...Args>
  int Assign(int i, Args ...args) {
    if(Size() == pool_.size()) {
      throw 1; //TODO: Actual exception
    }
    lock.lock();
    Thread<T> *thread = free_list_.front();
    free_list_.pop_front();
    ++size_;
    lock.unlock();
    thread->Assign(args...);
    return thread->GetTid();
  }

  void Stop(int tid) {
    pool_[tid].Stop();
    lock.lock();
    free_list_.emplace_back(&pool_[tid]);
    --size_;
    lock.unlock();
  }

  void StopAll() {
    for(int i = 0; i < pool_.size(); ++i) {
      Stop(i);
    }
  }

  void WaitAll() {
    for(int i = 0; i < pool_.size(); ++i) {
      pool_[i].Wait();
    }
  }

  uint32_t Size() {
    return size_;
  }

  uint32_t MaxSize() {
    return pool_.size();
  }

  Thread<T> &Get(int tid) {
    return pool_[tid];
  };

  std::shared_ptr<T> GetObj(int tid) {
    return std::move(pool_[tid].GetObj());
  };

  typename std::vector<Thread<T>>::iterator begin() {
    return pool_.begin();
  }

  typename std::vector<Thread<T>>::iterator end() {
    return pool_.end();
  }
};

}
#endif //COEUS_INCLUDE_COMMON_THREADPOOL_H_
