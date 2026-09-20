#ifndef _RCC_H
#define _RCC_H

#include "stdint.h"

typedef struct {
  uint32_t CR;
  uint32_t PLLCFGR;
  uint32_t CFGR;
  uint32_t CIR;
  uint32_t dummy1[8];
  uint32_t AHB1ENR;
  uint32_t dummy2[3];
  uint32_t APB1ENR;
} RCC_RegisterMap;

#define RCC ((RCC_RegisterMap *)0x40023800)

#define RCC_CR_HSION (1 << 16)
#define RCC_CR_HSIRDY (1 << 17)
#define RCC_CR_PLLON (1 << 24)
#define RCC_CR_PLLRDY (1 << 25)

#define RCC_PLLCFGR_PLLM_Pos 0
#define RCC_PLLCFGR_PLLN_Pos 6
#define RCC_PLLCFGR_PLLP_Pos 16
#define RCC_PLLCFGR_PLLQ_Pos 24

#define RCC_CFGR_SW_Pos 0
#define RCC_CFGR_SW_Msk (0x3 << RCC_CFGR_SW_Pos)
#define RCC_CFGR_SW_PLL (0b10 << RCC_CFGR_SW_Pos)

#define RCC_CFGR_SWS_Pos 2
#define RCC_CFGR_SWS_Msk (0x3 << RCC_CFGR_SWS_Pos)
#define RCC_CFGR_SWS_PLL (0b10 << RCC_CFGR_SWS_Pos)

#define RCC_CFGR_HPRE_Pos 4
#define RCC_CFGR_HPRE_Msk (0xF << RCC_CFGR_HPRE_Pos)

#define RCC_CFGR_PRE1_Pos 10
#define RCC_CFGR_PRE1_Msk (0x7 << RCC_CFGR_PRE1_Pos)

#define RCC_CFGR_PPRE1_DIV2 (0b100 << RCC_CFGR_PRE1_Pos)

#define RCC_CFGR_PRE2_Pos 13
#define RCC_CFGR_PRE2_Msk (0x7 << RCC_CFGR_PRE2_Pos)

#define RCC_AHB1ENR_GPIOAEN (1 << 0)
#define RCC_APB1ENR_USART2EN (1 << 17)

#endif // !_RCC_H
