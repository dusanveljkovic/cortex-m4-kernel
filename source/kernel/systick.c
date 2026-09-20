#include "../../include/systick.h"
#include "../../include/scb.h"
#include "../../include/scheduler.h"
#include <stdint.h>

volatile uint32_t system_ticks;

void systick_init() {
  STK->LOAD = 84000 - 1;
  STK->VAL = 0;
  STK->CTRL |= STK_CTRL_TICKINT | STK_CTRL_ENABLE | STK_CTRL_CLKSOURCE;
  system_ticks = 0;
}

void systick_handler(void) {
  system_ticks++;

  scheduler_put_task(current_task);
  SCB->ICSR |= ICSR_PENDSV_SET;
  asm volatile("dsb");
  asm volatile("isb");
}
