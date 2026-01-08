@CLAUDE.md Let's change the blocking strategy for wait, mutex, and corwlock.

# Future
Update the future to have a RunContext *parent_task raw pointer.
This should be nullptr by default

# IpcManager::Send
If we are on the runtime, we set the future's parent_task to be
the current task (from CHI_CUR_WORKER)

# Worker::AddToBlockedQueue
Add a new parameter to this function called ``bool wait_for_task``.
By default, wait_for_task should be false.
If wait_for_task is true, return and do not execute any other code in this function.
Otherwise, do the same code as before.

# Task::Wait
Remove the waiting_for_tasks variable from RunContext and its usages in Wait.
AddToBlockedQueue should set wait_for_task to true.
Call YieldBase in a do-while loop instead.

# Task::Yield
AddToBlockedQueue should set wait_for_task to false.

# Worker::Worker
Allocate an mpsc_queue<RunContext*> named event_queue_ from the main allocaotor 
with the same depth as the TaskLane for the worker.

# Worker::BeginTask
Add a pointer to the event_queue_ to the RunContext.

# Worker::ProcessEventQueue
Iterate over the event_queue_ using event_queue_.Pop.
Remove the RunContext* from the blocked_queue_ std::set.
Call ExecTask for each RunContext in the queue.

# Worker::ContinueBlockedTasks
Call ProcessEventQueue each iteration.

# Worker::EndTask
During EndTask, check if the Future's parent_task is non-null.
If so, enqueue parent_task inside the run_context->event_queue.
