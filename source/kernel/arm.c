#include "../../include/kernel/arm.h"
#include "../../include/kernel/bench.h"
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
#if BENCH
  bench_preemption_start = read_cycles();
#endif
  SCB->ICSR |= ICSR_PENDSV_SET;
  __DSB();
  __ISB();
}

#if BENCH_IRQ
void irq_restore(uint32_t primask) {
  if (!(primask & 1u)) {
    uint32_t d = DWT->CYCCNT - bench_irq_t0;
    update_irq_result(d, (uint32_t)__builtin_return_address(0));
  }
  asm volatile("msr primask, %0" : : "r"(primask) : "memory");
}
#endif
