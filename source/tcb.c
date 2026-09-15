#include "../include/tcb.h"
#include "../include/scheduler.h"
#include "../include/static_memory.h"
#include "../include/syscall.h"

#define N_TASKS 2

extern uint32_t *task_stack_init(uint32_t *stack_top, void (*func)(void));

void task_wrapper(void) {
  void (*func)(void *) = current_task->function;
  void *args = current_task->args;

  func(args);

  sys_task_exit();
}

tcb_t *create_task(uint8_t priority, void (*func)(void *), void *args) {
  tcb_t *task = alloc_task();
  if (task == 0)
    return 0;

  task->function = func;
  task->args = args;

  task->sp = task_stack_init(task->stack_top, task_wrapper);
  task->stack_size = 128;
  task->state = TASK_READY;
  task->priority = priority;

  return task;
}
