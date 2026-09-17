#include "../../include/systick.h"
#include <stdint.h>
void systick_init() {
  STK->LOAD = 8000000 - 1;
  STK->VAL = 0;
  STK->CTRL |= STK_CTRL_TICKINT | STK_CTRL_ENABLE | STK_CTRL_CLKSOURCE;
}
void __attribute__((weak)) systick_callback() {}
