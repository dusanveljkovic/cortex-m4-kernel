#ifndef _NVIC_H
#define _NVIC_H

#include <stdint.h>

typedef struct {
  uint32_t ISER[8];
  uint8_t dummy1[0x60];
  uint32_t ICER[8];
  uint8_t dummy2[0x60];
  uint32_t ISPR[8];
  uint8_t dummy3[0x60];
  uint32_t ICPR[8];
  uint8_t dummy4[0x60];
  uint32_t IABR[8];
  uint8_t dummy5[0xE0];
  uint8_t IPR[240];
} NVIC_RegisterMapType;

#define USART2_IRQ 38

#define NVIC ((NVIC_RegisterMapType *)0xE000E100)

#define NVIC_ENABLE_IRQ(irq_number)                                            \
  NVIC->ISER[irq_number / 32] |= (1 << (irq_number % 32))

#define NVIC_SET_PRIORITY(irq_number, priority)                                \
  NVIC->IPR[irq_number] = priority;

#define SVCALL_IRQ 11
#define PENDSV_IRQ 14
#define SYSTICK_IRQ 15

#endif // !_NVIC_H
