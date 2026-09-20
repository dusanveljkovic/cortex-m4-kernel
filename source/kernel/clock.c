#include "../../include/clock.h"
#include "../../include/flash.h"
#include "../../include/rcc.h"

void clock_init(void) {
  FLASH->ACR = FLASH_ACR_LATENCY_2WS | FLASH_ACR_ICEN | FLASH_ACR_DCEN;

  RCC->CR |= RCC_CR_HSION;

  while (!(RCC->CR & RCC_CR_HSIRDY))
    ;

  RCC->CFGR &= ~(RCC_CFGR_HPRE_Msk | RCC_CFGR_PRE1_Msk | RCC_CFGR_PRE2_Msk);

  RCC->CFGR |= RCC_CFGR_PPRE1_DIV2;

  RCC->PLLCFGR = (8 << RCC_PLLCFGR_PLLM_Pos) | (84 << RCC_PLLCFGR_PLLN_Pos) |
                 (0 << RCC_PLLCFGR_PLLP_Pos) | (4 << RCC_PLLCFGR_PLLQ_Pos);

  RCC->CR |= RCC_CR_PLLON;

  while (!(RCC->CR & RCC_CR_PLLRDY))
    ;

  RCC->CFGR &= ~RCC_CFGR_SW_Msk;
  RCC->CFGR |= RCC_CFGR_SW_PLL;

  while ((RCC->CFGR & RCC_CFGR_SWS_Msk) != RCC_CFGR_SWS_PLL)
    ;
}
