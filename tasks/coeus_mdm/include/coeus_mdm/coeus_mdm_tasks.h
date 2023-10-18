//
// Created by lukemartinlogan on 8/11/23.
//

#ifndef HRUN_TASKS_TASK_TEMPL_INCLUDE_coeus_mdm_coeus_mdm_TASKS_H_
#define HRUN_TASKS_TASK_TEMPL_INCLUDE_coeus_mdm_coeus_mdm_TASKS_H_

#include "hrun/api/hrun_client.h"
#include "hrun/task_registry/task_lib.h"
#include "hrun_admin/hrun_admin.h"
#include "hrun/queue_manager/queue_manager_client.h"
#include "proc_queue/proc_queue.h"

#include "common/DbOperation.h"

namespace hrun::coeus_mdm {

#include "coeus_mdm_methods.h"
#include "hrun/hrun_namespace.h"

using hrun::proc_queue::TypedPushTask;
using hrun::proc_queue::PushTask;

/**
 * A task to create coeus_mdm
 * */
using hrun::Admin::CreateTaskStateTask;
struct ConstructTask : public CreateTaskStateTask {
  IN hipc::ShmArchive<hipc::string> db_path_;
  /** SHM default constructor */
  HSHM_ALWAYS_INLINE explicit
  ConstructTask(hipc::Allocator *alloc)
  : CreateTaskStateTask(alloc) {}

  /** Emplace constructor */
  HSHM_ALWAYS_INLINE explicit
  ConstructTask(hipc::Allocator *alloc,
                const TaskNode &task_node,
                const DomainId &domain_id,
                const std::string &state_name,
                const TaskStateId &id,
                const std::vector<PriorityInfo> &queue_info,
                const std::string &db_path)
      : CreateTaskStateTask(alloc, task_node, domain_id, state_name,
                            "coeus_mdm", id, queue_info) {
    // Custom params
    HSHM_MAKE_AR(db_path_, alloc, db_path);
  }

  HSHM_ALWAYS_INLINE
  ~ConstructTask() {
    // Custom params
  }
};

/** A task to destroy coeus_mdm */
using hrun::Admin::DestroyTaskStateTask;
struct DestructTask : public DestroyTaskStateTask {
  /** SHM default constructor */
  HSHM_ALWAYS_INLINE explicit
  DestructTask(hipc::Allocator *alloc)
  : DestroyTaskStateTask(alloc) {}

  /** Emplace constructor */
  HSHM_ALWAYS_INLINE explicit
  DestructTask(hipc::Allocator *alloc,
               const TaskNode &task_node,
               const DomainId &domain_id,
               TaskStateId &state_id)
  : DestroyTaskStateTask(alloc, task_node, domain_id, state_id) {}

  /** Create group */
  HSHM_ALWAYS_INLINE
  u32 GetGroup(hshm::charbuf &group) {
    return TASK_UNORDERED;
  }
};

/**
 * A custom task in coeus_mdm
 * */
struct Mdm_insertTask : public Task, TaskFlags<TF_LOCAL> {
  IN hipc::ShmArchive<hipc::string> db_op_;

  /** SHM default constructor */
  HSHM_ALWAYS_INLINE explicit
  Mdm_insertTask(hipc::Allocator *alloc) : Task(alloc) {}

  /** Emplace constructor */
  HSHM_ALWAYS_INLINE explicit
  Mdm_insertTask(hipc::Allocator *alloc,
                 const TaskNode &task_node,
                 const DomainId &domain_id,
                 const TaskStateId &state_id,
                 const DbOperation &db_op) : Task(alloc) {
    // Initialize task
    task_node_ = task_node;
    lane_hash_ = 0;
    prio_ = TaskPrio::kLowLatency;
    task_state_ = state_id;
    method_ = Method::kMdm_insert;
    task_flags_.SetBits(TASK_FIRE_AND_FORGET);
    domain_id_ = domain_id;

    // Store DbOperation
    std::stringstream ss;
    cereal::BinaryOutputArchive ar(ss);
    ar << db_op;
    std::string db_op_ser = ss.str();
    HSHM_MAKE_AR(db_op_, alloc, db_op_ser);
  }

  ~Mdm_insertTask(){
    HSHM_DESTROY_AR(db_op_);
  }

  DbOperation GetDbOp(){
    DbOperation db_op;
    std::stringstream ss(db_op_->str());
    cereal::BinaryInputArchive ar(ss);
    ar >> db_op;
    return db_op;
  }

  /** Create group */
  HSHM_ALWAYS_INLINE
  u32 GetGroup(hshm::charbuf &group) {
    return TASK_UNORDERED;
  }
};

}  // namespace hrun::coeus_mdm

#endif  // HRUN_TASKS_TASK_TEMPL_INCLUDE_coeus_mdm_coeus_mdm_TASKS_H_
