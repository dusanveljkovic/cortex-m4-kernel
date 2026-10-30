#include "../../include/kernel/arm.h"
#include "../../include/kernel/scb.h"
#include "../../include/kernel/scheduler.h"

inline void request_context_switch(void) {
  tcb_t *candidate = scheduler_next_task();
  uint32_t ipsr = get_IPSR();
  if (SCB_PENDSV_PENDING || (ipsr == 14) // pendsv is currently running
      || candidate == 0 ||
      (current_task->state == TASK_RUNNING &&
       candidate->effective_priority > current_task->effective_priority))
    return;
  SCB->ICSR |= ICSR_PENDSV_SET;
  __DSB();
  __ISB();
}
