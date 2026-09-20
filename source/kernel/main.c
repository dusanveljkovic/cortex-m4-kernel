#include "../../include/clock.h"
#include "../../include/heap.h"
#include "../../include/scb.h"
#include "../../include/scheduler.h"
#include "../../include/static_memory.h"
#include "../../include/tcb.h"
#include "../../include/usart.h"

static uint32_t data = 0;

extern void user_main(void *);
extern uint8_t _heap_start;
extern uint8_t _heap_end;
int main() {
  clock_init();
  usart2_init();

  usart2_puts("HELLO\r\n");

  usart2_test();

  static_memory_init();
  heap_init(&_heap_start, (uint32_t)(&_heap_end - &_heap_start));
  scheduler_init();

  SCB->SHPR2 = 0xE0000000;
  SCB->SHPR3 = 0xE0F00000;
  tcb_t *user_task = create_task(0, user_main, 0);
  scheduler_put_task(user_task);

  SCB->ICSR |= ICSR_PENDSV_SET;
  asm volatile("dsb");
  asm volatile("isb");

  // systick_init();

  while (1) {
  }

  return 0;
}
