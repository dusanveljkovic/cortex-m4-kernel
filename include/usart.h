#ifndef _USART_H
#define _USART_H

#include "stdint.h"
typedef struct {
  uint32_t SR;
  uint32_t DR;
  uint32_t BRR;
  uint32_t CR1;
  uint32_t CR2;
  uint32_t CR3;
  uint32_t GTPR;
} USART_RegisterMap;

#define USART2 ((USART_RegisterMap *)0x40004400)

#define USART_ENABLE_TE (1 << 3)
#define USART_ENABLE_RE (1 << 2)
#define USART_ENABLE (1 << 13)

#define USART_SR_TXE (1 << 7)

void usart2_init(void);
void usart2_puts(const char *str);
void usart2_test(void);

#endif // !_USART_H
