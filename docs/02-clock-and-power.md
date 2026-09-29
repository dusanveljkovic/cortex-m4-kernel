# Clock & Power

**Files:** `source/kernel/clock.c`, `include/clock.h`, `include/rcc.h`,
`include/flash.h`

## What it configures

`clock_init()` brings the core from its reset default (16 MHz HSI, direct)
up to a PLL-driven **84 MHz**, with matching bus clocks:

| Clock  | Frequency | How |
|--------|-----------|-----|
| SYSCLK / HCLK | 84 MHz | PLL output, AHB prescaler `/1` |
| APB1 (PCLK1)  | 42 MHz | APB1 prescaler `/2` (bus max is 45 MHz) |
| APB2 (PCLK2)  | 84 MHz | APB2 prescaler `/1` (bus max is 90 MHz) |

**PLL math**, source = HSI (16 MHz):
```
VCO input  = HSI / PLLM        = 16 / 8   = 2 MHz
VCO output = VCO input * PLLN  = 2 * 84    = 168 MHz
SYSCLK     = VCO output / PLLP = 168 / 2   = 84 MHz     (PLLP field 0b00 = /2)
```

## Initializing sequence explained

```c
void clock_init(void) {
  FLASH->ACR = FLASH_ACR_LATENCY_2WS | FLASH_ACR_ICEN | FLASH_ACR_DCEN;

  RCC->CR |= RCC_CR_HSION;
  while (!(RCC->CR & RCC_CR_HSIRDY));

  RCC->CFGR &= ~(RCC_CFGR_HPRE_Msk | RCC_CFGR_PRE1_Msk | RCC_CFGR_PRE2_Msk);
  RCC->CFGR |= RCC_CFGR_PPRE1_DIV2;

  RCC->PLLCFGR = (8 << RCC_PLLCFGR_PLLM_Pos) | (84 << RCC_PLLCFGR_PLLN_Pos) |
                 (0 << RCC_PLLCFGR_PLLP_Pos) | (4 << RCC_PLLCFGR_PLLQ_Pos);
  RCC->CR |= RCC_CR_PLLON;
  while (!(RCC->CR & RCC_CR_PLLRDY));

  RCC->CFGR &= ~RCC_CFGR_SW_Msk;
  RCC->CFGR |= RCC_CFGR_SW_PLL;
  while ((RCC->CFGR & RCC_CFGR_SWS_Msk) != RCC_CFGR_SWS_PLL);
}
```

1. **Flash latency is set first, before the clock speeds up.** At 84 MHz
   (60–90 MHz band, per the reference manual's wait-state table), flash needs
   2 wait states to keep up with fetches - setting this *after* switching
   would risk a fetch failure at a speed flash isn't configured for yet.
   Prefetch isn't enabled here, but the instruction and data caches
   (`ICEN`/`DCEN`) are.
2. **HSI is confirmed ready** before it's used as the PLL input.
3. **Bus prescalers are set before the PLL is enabled** - APB1 needs to divide
   by 2 to stay under its 45 MHz cap once the 84 MHz clock is live; setting it
   in the same breath as the PLL avoids a window where a bus could briefly
   see an out-of-spec clock.
4. **The PLL is started and confirmed locked (`PLLRDY`)** before anything
   tries to switch to it.
5. **Only then does `SW` actually switch SYSCLK to the PLL**, confirmed by
   polling `SWS`. 

## Voltage scaling

There's no `PWR->CR` voltage-scale write here, and none is needed: 84 MHz
falls comfortably inside the default (POR) Scale 3 range, which supports up
to 120 MHz. Voltage scaling only becomes necessary if this clock
configuration is ever pushed past that point (STM32F446 tops out at 180 MHz
with Scale 1 + over-drive).
