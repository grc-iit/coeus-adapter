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

#include "hermes/hermes.h"

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
  IN TaskStateId hermes_id_;
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
    hermes_id_ = HERMES_CONF->blob_mdm_.id_;



    HSHM_MAKE_AR(db_path_, alloc, db_path);
    std::stringstream ss;
    cereal::BinaryOutputArchive ar(ss);
    ar(db_path_, hermes_id_);
    std::string data = ss.str();
    *custom_ = data;
  }

  void Deserialize() {
    std::string data = custom_->str();
    std::stringstream ss(data);
    cereal::BinaryInputArchive ar(ss);
    ar(db_path_, hermes_id_);
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

struct Put_hashTask : public Task, TaskFlags<TF_LOCAL> {
  IN TagId tag_id_;
  IN hipc::ShmArchive<hipc::charbuf> blob_name_;
  IN size_t blob_off_;
  IN size_t data_size_;
  IN hipc::Pointer data_;
  IN float score_;
  IN bitfield32_t flags_;
  IN BlobId blob_id_;
  /** SHM default constructor */
  HSHM_ALWAYS_INLINE explicit
  Put_hashTask(hipc::Allocator *alloc) : Task(alloc) {}

  /** Emplace constructor */
  HSHM_ALWAYS_INLINE explicit
  Put_hashTask(hipc::Allocator *alloc,
                 const TaskNode &task_node,
                 const DomainId &domain_id,
                 const TaskStateId &state_id,
                 const TagId &tag_id,
                 const hshm::charbuf &blob_name,
                 const BlobId &blob_id,
                 size_t blob_off,
                 size_t data_size,
                 const hipc::Pointer &data,
                 float score,
                 u32 flags,
                 const Context &ctx,
                 u32 task_flags) : Task(alloc) {
    // Initialize task

    task_node_ = task_node;
    lane_hash_ = 0;
    prio_ = TaskPrio::kLowLatency;
    task_state_ = state_id;
    method_ = Method::kPut_hash;
    task_flags_ = bitfield32_t(task_flags);
    task_flags_.SetBits(TASK_COROUTINE);
    if (!blob_id.IsNull()) {
      lane_hash_ = blob_id.hash_;
      domain_id_ = domain_id;
    } else {
      lane_hash_ = HashBlobName(tag_id, blob_name);
      domain_id_ = DomainId::GetNode(HASH_TO_NODE_ID(lane_hash_));
    }

    // Custom params
    tag_id_ = tag_id;
    HSHM_MAKE_AR(blob_name_, alloc, blob_name);
    blob_id_ = blob_id;
    blob_off_ = blob_off;
    data_size_ = data_size;
    data_ = data;
    score_ = score;
    flags_ = bitfield32_t(flags | ctx.flags_.bits_);
  }

  ~Put_hashTask() {
    HSHM_DESTROY_AR(blob_name_);
//    if (IsDataOwner()) {
//      // HILOG(kInfo, "Actually freeing PUT {} of size {}", task_node_, data_size_);
//      HRUN_CLIENT->FreeBuffer(data_);
//    }
  }

  /** (De)serialize message call */
  template<typename Ar>
  void SaveStart(Ar &ar) {
    DataTransfer xfer(DT_RECEIVER_READ,
                      HERMES_MEMORY_MANAGER->Convert<char>(data_),
                      data_size_, domain_id_);
    task_serialize<Ar>(ar);
    ar & xfer;
    ar(tag_id_, blob_name_, blob_id_, blob_off_, data_size_, score_, flags_);
  }

  /** Deserialize message call */
  template<typename Ar>
  void LoadStart(Ar &ar) {
    DataTransfer xfer;
    task_serialize<Ar>(ar);
    ar & xfer;
    data_ = HERMES_MEMORY_MANAGER->Convert<void, hipc::Pointer>(xfer.data_);
    ar(tag_id_, blob_name_, blob_id_, blob_off_, data_size_, score_, flags_);
  }

  /** (De)serialize message return */
  template<typename Ar>
  void SerializeEnd(u32 replica, Ar &ar) {
    if (flags_.Any(HERMES_GET_BLOB_ID)) {
      ar(blob_id_);
    }
  }

  /** Create group */
  HSHM_ALWAYS_INLINE
  u32 GetGroup(hshm::charbuf &group) {
    return TASK_UNORDERED;
  }
};


}  // namespace hrun::coeus_mdm

#endif  // HRUN_TASKS_TASK_TEMPL_INCLUDE_coeus_mdm_coeus_mdm_TASKS_H_
