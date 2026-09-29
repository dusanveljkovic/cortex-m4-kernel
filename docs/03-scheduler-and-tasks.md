# Scheduler & Tasks

**Files:** `source/kernel/scheduler.c`, `source/kernel/tcb.c`,
`source/kernel/systick.c`, `source/kernel/sleep.c`,
`source/kernel/task_stack_init.s`, `source/kernel/pend_sv_handler.s`,
`include/scheduler.h`, `include/tcb.h`, `include/systick.h`,
`include/sleep.h`

## Task states

```c
typedef enum {
  TASK_UNUSED = 0, TASK_READY, TASK_RUNNING, TASK_BLOCKED,
  TASK_SUSPENDED, TASK_SLEEPING, TASK_FINISHED
} task_state_t;
```

`TASK_UNUSED` marks a free slot in the static task pool.

## The TCB

```c
typedef struct tcb {
  uint32_t *sp;                 // saved stack pointer (only valid while not running)
  task_state_t state;
  uint8_t base_priority;         // as given to create_task; 0 = highest
  uint8_t effective_priority;    // may be temporarily boosted by mutex priority inheritance
  uint8_t *stack_base;           // for the MPU: this task's stack region
  uint32_t stack_size;
  struct tcb *next;              // intrusive linked-list pointer (ready queue / wait queue)
  const char *name;
  void (*function)(void *);
  void *args;
  uint8_t unprivileged;          // 1 unless explicitly cleared (see tcb.c)
  uint32_t wake_tick;            // set by sleep_for_ticks()
  void *waiting_on;               // the semaphore/mutex this task is blocked on, if any
  struct mutex *owned_mutexes;    // linked list of mutexes this task currently holds
} tcb_t;
```

Priority is a single byte where **lower numbers mean higher priority**. 
## Creating a task

`create_task(priority, func, args)`:
1. Pulls a free slot from the static pool (`alloc_task()`,
   see [Memory Management](06-memory-management.md)).
2. Sets `function`/`args`/`base_priority`/`effective_priority` and marks
   `unprivileged = 1` 
3. Calls into `task_stack_init.s` to synthesize an initial exception stack
   frame on the task's own stack - the same 8-word layout the CPU
   automatically pushes on a real exception, plus the callee-saved
   `r4`-`r11` a context switch expects to restore. This makes the very first
   switch into a brand-new task indistinguishable, from the context-switch
   code's point of view, from switching into a task that's merely been
   interrupted before.
4. The task doesn't start executing `function` directly — its stack frame's
   `PC` points at a small trampoline that calls `function(args)` and then
   calls `sys_task_exit()` if `function` ever returns, so a task "finishing"
   is handled the same way regardless of whether it loops forever or returns.

A created task need to be queued in order to run by explicitly calling 
`scheduler_put_task()` from privileged code. The syscall handler `SYS_TASK_CREATE` 
does this automatically so unprivileged code can still create and queue a task.

## The ready queue

A single priority-ordered linked list. Tasks link through their own `next` field 
so there's no separate node allocation.

- **`priority_queue_push`** inserts in priority order (lower
  `effective_priority` first), scanning from the head. Same-priority tasks
  end up FIFO among themselves, because the scan condition
  (`current->next->effective_priority <= task->effective_priority`) only
  stops at a *strictly* lower-priority successor.
- **`priority_queue_pop`** - get the head (highest priority task)
- **`priority_queue_remove`** / **`priority_queue_reoder`** support pulling a
  task out mid-queue and re-inserting it at its (possibly changed) priority -
  used when a mutex unlock boosts or restores a task's `effective_priority`
  while it's still sitting in the queue.
- **`scheduler_put_task`** refuses to enqueue a task that's `TASK_FINISHED`,
  `TASK_BLOCKED`, or `TASK_SLEEPING` 

All of the above run inside `irq_save()`/`irq_restore()` - see the
[Synchronization](05-synchronization.md) doc's note on why this matters even
in a system with only one core.

## Context switching

`scheduler_select_next()` just pops the ready queue (falling back to
re-running `current_task` if the queue is empty - there's always at least the
`IDLE` task in it in practice, so this fallback is mostly a safety net) and
marks the result `TASK_RUNNING`. The actual register save/restore is
`pend_sv_handler.s`'s job, at the lowest exception priority on the chip (see
[Boot & Exceptions](01-boot-and-exceptions.md)) - it's the one place that
directly touches `current_task`'s saved stack pointer.

## The tick: preemption and sleep

`systick_init()` configures a 1 ms tick (`STK->LOAD = 84000 - 1` against the
84 MHz core clock; see [Clock & Power](02-clock-and-power.md)).
`systick_handler`, on every tick:
1. Increments a tick counter.
2. Calls `sleep_tick()` (`sleep.c`), which wakes any tasks whose
   `wake_tick` has arrived and re-queues them.
3. Re-queues `current_task` (round-robining it behind any other ready task at
   the same priority) and pends `PendSV`

`sleep_for_ticks(ticks)` (used by the `SYS_SLEEP` syscall) marks the calling
task `TASK_SLEEPING`, computes its `wake_tick`, and inserts it into a
tick-ordered wait list (`sleep_list`) - a task doesn't sit in the ready queue
while it's sleeping, so it costs the scheduler nothing to skip over.

