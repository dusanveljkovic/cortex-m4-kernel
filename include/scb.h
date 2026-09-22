#ifndef _SCB_H
#define _SCB_H

#include <stdint.h>

typedef struct {
  uint32_t CPUID;
  uint32_t ICSR;
  uint32_t VTOR;
  uint32_t AIRCR;
  uint32_t SCR;
  uint32_t CCR;
  uint32_t SHPR[3];
  uint32_t SHCSR;
  uint32_t CFSR;
  uint32_t HFSR;
  uint32_t dummy0[1];
  uint32_t MMFAR;
  uint32_t BFAR;

} SCB_RegisterMapType;

#define SCB ((SCB_RegisterMapType *)0xE000ED00)

#define SBC_CCR_DIV_0 (1 << 4)
#define SCB_SHCSR_USG_FAULT_ENABLE (1 << 18)
#define SCB_SHCSR_MEMFAULTENABLE (1 << 16)
#define ICSR_PENDSV_SET (1 << 28)

#define SYSTICK_SHPR 3
#define PENDSV_SHPR 3
#define SVCALL_SHPR 2

#define SYSTICK_PRIORITY_POS 24
#define PENDSV_PRIORITY_POS 16
#define SVCALL_PRIORITY_POS 24

#define SCB_SET_PRIORITY(shpr, pos, priority)                                  \
  SCB->SHPR[shpr - 1] &= ~(0xFF << pos);                                       \
  SCB->SHPR[shpr - 1] |= (priority << pos);

#endif // !_SCB_H
