# Synchronization: Semaphores & Mutexes

**Files:** `source/kernel/semaphore.c`, `include/semaphore.h`

Both semaphores and mutexes are heap-allocated (`SYS_SEM_CREATE`/
`SYS_MUTEX_CREATE` call `kmalloc`), not drawn from a static pool.

## Semaphores

A standard counting semaphore: `count`, a FIFO wait queue, and a `state`
(`SEM_UNUSED` / `SEM_OPEN` / `SEM_CLOSED`).

- **`semaphore_wait`**: if `count > 0`, decrements and returns immediately
  (`SEM_OK`). Otherwise, blocks the calling task (`TASK_BLOCKED`,
  `waiting_on = sem`) and pushes it onto the semaphore's wait queue, returning
  `SEM_OK_YIELD` so the syscall dispatcher knows to actually reschedule.
- **`semaphore_post`**: if a task is waiting, pops the head of the wait queue,
  marks it `TASK_READY`, and re-queues it via `scheduler_put_task` - a task
  waiting on a semaphore is handed the "unit" directly rather than the count
  being incremented and later decremented again. Only if nobody was waiting
  does `count` actually increment.
- **`semaphore_close`**: drains the entire wait queue, waking every blocked
  task (with `waiting_on` cleared *before* re-queuing, not after), then marks
  the semaphore `SEM_CLOSED`. A woken task doesn't automatically know the
  semaphore closed out from under it - the syscall path just resumes it with
  whatever return value was already queued up (`SEM_OK_YIELD`, decided when
  it first blocked), so code that waits on a semaphore should be prepared to
  find it closed if that's a real possibility in your application.

Every operation runs inside `irq_save()`/`irq_restore()`.

## Mutexes: priority inheritance

A plain lock/unlock mutex would let a low-priority task holding a lock block
a high-priority task waiting on it indefinitely, while medium-priority tasks
that don't even want the lock keep preempting the low-priority holder This mutex 
implementation avoids the common case of that by **temporarily boosting the holder's priority** 
to match the highest-priority waiter:

```c
sem_result_t mutex_lock(mutex_t *m) {
  ...
  if (task->effective_priority < m->owner->effective_priority) {
    m->owner->effective_priority = task->effective_priority;
    if (m->owner->state == TASK_READY)
      scheduler_reorder(m->owner);
  }
  ...
}
```

If the owner is currently sitting in the ready queue (not actually running),
`scheduler_reorder` pulls it out and re-inserts it at its new, higher
priority so it's scheduled sooner. `mutex_unlock` reverses the boost when the
mutex is released, via `task_recalculate_priority` (in `tcb.c`), which scans
everything the task *still* owns and sets `effective_priority` back to the
highest boost still justified.

A task's currently-held mutexes form a linked list through
`owned_mutexes`/`next_owned` (`mutex_queue_push`/`_remove`, private to
`semaphore.c`) — this is what `task_recalculate_priority` walks, and what the
CLI's `mutex` command walks to print ownership.

Locking your own already-held mutex returns `MUTEX_DEADLOCK` rather than
blocking forever. Unlocking a mutex you don't own returns `MUTEX_NOT_OWNER`.
Note there's no `SYS_MUTEX_CLOSE` syscall wired up — `mutex_close` exists in
the header but nothing currently calls it.
