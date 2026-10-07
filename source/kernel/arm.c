#include "../../include/arm.h"
#include "../../include/scb.h"
#include "../../include/scheduler.h"

inline void request_context_switch(void) {
  tcb_t *next_task = scheduler_next_task();
  if (SCB_PENDSV_PENDING || next_task == 0 ||
      (current_task->state == TASK_RUNNING &&
       next_task->effective_priority > current_task->effective_priority))
    return;
  scheduler_put_task(current_task);
  SCB->ICSR |= ICSR_PENDSV_SET;
  __DSB();
  __ISB();
}
