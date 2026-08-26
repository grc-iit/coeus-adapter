/**
 * Auto-generated execution implementation for rankConsensus ChiMod (clio-core port).
 * Implements Container virtual APIs (Run, SaveTask, LoadTask, NewCopyTask,
 * NewTask, ...) using switch-case dispatch.
 *
 * Hand-ported to the clio-core (iowarp-core 2.1.0) Container API following the
 * bdev/simple_mod reference modules.
 */

#include "coeus/rankConsensus/rankConsensus_runtime.h"
#include "coeus/rankConsensus/autogen/rankConsensus_methods.h"

#include <clio_runtime/clio_runtime.h>
#include <clio_runtime/task.h>  // For TaskResume coroutine return type

namespace coeus::rankConsensus {

//==============================================================================
// Container Virtual API Implementations
//==============================================================================

void Runtime::Init(const clio::run::PoolId& pool_id, const std::string& pool_name,
                   clio::run::u32 container_id) {
  clio::run::Container::Init(pool_id, pool_name, container_id);
  client_ = Client(pool_id);
}

clio::run::TaskResume Runtime::Run(clio::run::u32 method,
                                   clio::run::shared_ptr<clio::run::Task> task_ptr) {
  CLIO_TASK_BODY_BEGIN
  switch (method) {
    case Method::kCreate: {
      auto& typed_task = task_ptr.template Cast<CreateTask>();
      CLIO_CO_AWAIT(Create(typed_task));
      break;
    }
    case Method::kDestroy: {
      auto& typed_task = task_ptr.template Cast<DestroyTask>();
      CLIO_CO_AWAIT(Destroy(typed_task));
      break;
    }
    case Method::kGetRank: {
      auto& typed_task = task_ptr.template Cast<GetRankTask>();
      CLIO_CO_AWAIT(GetRank(typed_task));
      break;
    }
    default: {
      break;
    }
  }
  CLIO_CO_RETURN;
  CLIO_TASK_BODY_END
}

void Runtime::SaveTask(clio::run::u32 method, clio::run::SaveTaskArchive& archive,
                       clio::run::shared_ptr<clio::run::Task>& task_ptr) {
  switch (method) {
    case Method::kCreate: {
      auto& typed_task = task_ptr.template Cast<CreateTask>();
      archive << *typed_task;
      break;
    }
    case Method::kDestroy: {
      auto& typed_task = task_ptr.template Cast<DestroyTask>();
      archive << *typed_task;
      break;
    }
    case Method::kGetRank: {
      auto& typed_task = task_ptr.template Cast<GetRankTask>();
      archive << *typed_task;
      break;
    }
    default: {
      break;
    }
  }
}

void Runtime::LoadTask(clio::run::u32 method, clio::run::LoadTaskArchive& archive,
                       clio::run::shared_ptr<clio::run::Task>& task_ptr) {
  switch (method) {
    case Method::kCreate: {
      auto& typed_task = task_ptr.template Cast<CreateTask>();
      archive >> *typed_task;
      break;
    }
    case Method::kDestroy: {
      auto& typed_task = task_ptr.template Cast<DestroyTask>();
      archive >> *typed_task;
      break;
    }
    case Method::kGetRank: {
      auto& typed_task = task_ptr.template Cast<GetRankTask>();
      archive >> *typed_task;
      break;
    }
    default: {
      break;
    }
  }
}

clio::run::shared_ptr<clio::run::Task> Runtime::AllocLoadTask(
    clio::run::u32 method, clio::run::LoadTaskArchive& archive) {
  clio::run::shared_ptr<clio::run::Task> task_ptr = NewTask(method);
  if (!task_ptr.IsNull()) {
    LoadTask(method, archive, task_ptr);
  }
  return task_ptr;
}

void Runtime::LocalLoadTask(clio::run::u32 method, clio::run::DefaultLoadArchive& archive,
                            clio::run::shared_ptr<clio::run::Task>& task_ptr) {
  switch (method) {
    case Method::kCreate: {
      auto& typed_task = task_ptr.template Cast<CreateTask>();
      archive >> *typed_task;
      break;
    }
    case Method::kDestroy: {
      auto& typed_task = task_ptr.template Cast<DestroyTask>();
      archive >> *typed_task;
      break;
    }
    case Method::kGetRank: {
      auto& typed_task = task_ptr.template Cast<GetRankTask>();
      archive >> *typed_task;
      break;
    }
    default: {
      break;
    }
  }
}

clio::run::shared_ptr<clio::run::Task> Runtime::LocalAllocLoadTask(
    clio::run::u32 method, clio::run::DefaultLoadArchive& archive) {
  clio::run::shared_ptr<clio::run::Task> task_ptr = NewTask(method);
  if (!task_ptr.IsNull()) {
    LocalLoadTask(method, archive, task_ptr);
  }
  return task_ptr;
}

void Runtime::LocalSaveTask(clio::run::u32 method, clio::run::DefaultSaveArchive& archive,
                            clio::run::shared_ptr<clio::run::Task>& task_ptr) {
  switch (method) {
    case Method::kCreate: {
      auto& typed_task = task_ptr.template Cast<CreateTask>();
      archive << *typed_task;
      break;
    }
    case Method::kDestroy: {
      auto& typed_task = task_ptr.template Cast<DestroyTask>();
      archive << *typed_task;
      break;
    }
    case Method::kGetRank: {
      auto& typed_task = task_ptr.template Cast<GetRankTask>();
      archive << *typed_task;
      break;
    }
    default: {
      break;
    }
  }
}

clio::run::shared_ptr<clio::run::Task> Runtime::NewCopyTask(
    clio::run::u32 method, clio::run::shared_ptr<clio::run::Task>& orig_task_ptr,
    bool deep) {
  auto* ipc_manager = CLIO_IPC;
  if (!ipc_manager) {
    return clio::run::shared_ptr<clio::run::Task>();
  }

  switch (method) {
    case Method::kCreate: {
      auto& task_typed = orig_task_ptr.template Cast<CreateTask>();
      auto new_task_ptr = ipc_manager->NewTask<CreateTask>();
      if (!new_task_ptr.IsNull()) {
        new_task_ptr->Copy(ctp::ipc::FullPtr<CreateTask>(task_typed.get()));
        return new_task_ptr.template Cast<clio::run::Task>();
      }
      break;
    }
    case Method::kDestroy: {
      auto& task_typed = orig_task_ptr.template Cast<DestroyTask>();
      auto new_task_ptr = ipc_manager->NewTask<DestroyTask>();
      if (!new_task_ptr.IsNull()) {
        new_task_ptr->Copy(ctp::ipc::FullPtr<DestroyTask>(task_typed.get()));
        return new_task_ptr.template Cast<clio::run::Task>();
      }
      break;
    }
    case Method::kGetRank: {
      auto& task_typed = orig_task_ptr.template Cast<GetRankTask>();
      auto new_task_ptr = ipc_manager->NewTask<GetRankTask>();
      if (!new_task_ptr.IsNull()) {
        new_task_ptr->Copy(ctp::ipc::FullPtr<GetRankTask>(task_typed.get()));
        return new_task_ptr.template Cast<clio::run::Task>();
      }
      break;
    }
    default: {
      auto new_task_ptr = ipc_manager->NewTask<clio::run::Task>();
      if (!new_task_ptr.IsNull()) {
        new_task_ptr->Copy(ctp::ipc::FullPtr<clio::run::Task>(orig_task_ptr.get()));
        return new_task_ptr;
      }
      break;
    }
  }

  (void)deep;
  return clio::run::shared_ptr<clio::run::Task>();
}

clio::run::shared_ptr<clio::run::Task> Runtime::NewTask(clio::run::u32 method) {
  auto* ipc_manager = CLIO_IPC;
  if (!ipc_manager) {
    return clio::run::shared_ptr<clio::run::Task>();
  }

  switch (method) {
    case Method::kCreate: {
      auto new_task_ptr = ipc_manager->NewTask<CreateTask>();
      return new_task_ptr.template Cast<clio::run::Task>();
    }
    case Method::kDestroy: {
      auto new_task_ptr = ipc_manager->NewTask<DestroyTask>();
      return new_task_ptr.template Cast<clio::run::Task>();
    }
    case Method::kGetRank: {
      auto new_task_ptr = ipc_manager->NewTask<GetRankTask>();
      return new_task_ptr.template Cast<clio::run::Task>();
    }
    default: {
      return clio::run::shared_ptr<clio::run::Task>();
    }
  }
}

void Runtime::AggregateOut(clio::run::u32 method,
                           clio::run::shared_ptr<clio::run::Task>& orig_task,
                           const clio::run::shared_ptr<clio::run::Task>& replica_task) {
  switch (method) {
    default: {
      orig_task->AggregateOut(ctp::ipc::FullPtr<clio::run::Task>(replica_task.get()));
      break;
    }
  }
}

}  // namespace coeus::rankConsensus
