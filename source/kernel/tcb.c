#include "../../include/tcb.h"
#include "../../include/arm.h"
#include "../../include/scheduler.h"
#include "../../include/static_memory.h"
#include "../../include/syscall.h"

extern uint32_t *task_stack_init(uint32_t *stack_top, void (*func)(void *),
                                 void *args);

__attribute__((noreturn)) void task_wrapper(void (*func)(void *), void *args) {
  func(args);

  sys_task_exit();

  while (1)
    ;
}

tcb_t *create_task(uint8_t priority, void (*func)(void *), void *args) {
  tcb_t *task = alloc_task();
  if (task == 0)
    return 0;

  task->function = func;
  task->args = args;

  task->sp = task_stack_init((uint32_t *)(task->stack_base + task->stack_size),
                             func, args);
  task->state = TASK_READY;
  task->base_priority = priority;
  task->effective_priority = priority;
  task->owned_mutexes = 0;
  task->unprivileged = 1;

  return task;
}

uint8_t task_recalculate_priority(tcb_t *task) {
  uint8_t priority = task->base_priority;

  mutex_t *mutex = task->owned_mutexes;

  while (mutex != 0) {
    tcb_t *waiter = mutex->wait_queue.head;

    if (waiter != 0 && priority > waiter->effective_priority) {
      priority = waiter->effective_priority;
    }

    mutex = mutex->next_owned;
  }
  return priority;
}

void task_set_privilege(tcb_t *task) {
  uint32_t control = 0;

  // set PSP as stack pointer
  control |= (1 << 1);

  control |= (task->unprivileged << 0);

  __SET_CONTROL(control);
  __ISB();
}
