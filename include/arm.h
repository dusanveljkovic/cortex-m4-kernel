#ifndef _ARM_H
#define _ARM_H

#include "stdint.h"

static inline uint32_t irq_save(void) {
  uint32_t primask;

  asm volatile("mrs %0, primask\n"
               "cpsid i"
               : "=r"(primask)
               :
               : "memory");
  return primask;
}

static inline void irq_restore(uint32_t primask) {
  asm volatile("msr primask, %0" : : "r"(primask) : "memory");
}

#define __DSB() asm volatile("dsb");
#define __ISB() asm volatile("isb");
#define __SET_CONTROL(control)                                                 \
  asm volatile("msr control, %0" : : "r"(control) : "memory");

#endif // !_ARM_H
