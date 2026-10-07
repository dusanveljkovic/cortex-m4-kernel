#include "../../include/systick.h"
#include "../../include/arm.h"
#include "../../include/sleep.h"

volatile uint32_t system_ticks;

void systick_init() {
  STK->LOAD = 84000 - 1;
  STK->VAL = 0;
  STK->CTRL |= STK_CTRL_TICKINT | STK_CTRL_ENABLE | STK_CTRL_CLKSOURCE;
  system_ticks = 0;
}

void systick_handler(void) {
  system_ticks++;
  sleep_tick();

  request_context_switch();
}
