#ifndef _ARM_H
#define _ARM_H

#include "bench.h"
#include "stdint.h"

static inline uint32_t irq_save(void) {
  uint32_t primask;

  asm volatile("mrs %0, primask\n"
               "cpsid i"
               : "=r"(primask)
               :
               : "memory");
#if BENCH_IRQ
  if (!(primask & 1u))
    bench_irq_t0 = DWT->CYCCNT;
#endif
  return primask;
}
#if BENCH_IRQ
__attribute__((noinline))
#endif
static inline void irq_restore(uint32_t primask) {
#if BENCH_IRQ
  if (!(primask & 1u)) {
    uint32_t d = DWT->CYCCNT - bench_irq_t0;
    update_irq_result(d, (uint32_t)__builtin_return_address(0));
  }
#endif
  asm volatile("msr primask, %0" : : "r"(primask) : "memory");
}
void request_context_switch(void);

static inline uint32_t get_IPSR() {
  uint32_t ipsr;
  asm volatile("mrs %0, ipsr" : "=r"(ipsr) : : "memory");
  return ipsr;
}

#define __DSB() asm volatile("dsb");
#define __ISB() asm volatile("isb");
#define __SET_CONTROL(control)                                                 \
  asm volatile("msr control, %0" : : "r"(control) : "memory");
static inline uint32_t get_CONTROL() {
  uint32_t control;
  asm volatile("mrs %0, control" : "=r"(control) : : "memory");
  return control;
}

#endif // !_ARM_H
