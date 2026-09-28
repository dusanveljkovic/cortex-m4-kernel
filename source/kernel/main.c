#include "../../include/cli.h"
#include "../../include/clock.h"
#include "../../include/console.h"
#include "../../include/heap.h"
#include "../../include/kprintf.h"
#include "../../include/mpu.h"
#include "../../include/nvic.h"
#include "../../include/scb.h"
#include "../../include/scheduler.h"
#include "../../include/static_memory.h"
#include "../../include/systick.h"
#include "../../include/tcb.h"
#include "../drivers/spi.h"
#include "../drivers/usart.h"

static uint32_t data = 0;

extern void user_main(void *);
extern uint8_t _heap_start;
extern uint8_t _heap_end;

void idle_func(void *a) {
  while (1)
    ;
}

void create_tasks(void);
int main() {
  clock_init();
  driver_register(&usart2_driver);
  console_attach(driver_find("usart2"));

  static_memory_init();
  heap_init(&_heap_start, (uint32_t)(&_heap_end - &_heap_start));
  scheduler_init();
  mpu_init();

  NVIC_SET_PRIORITY(USART2_IRQ, 0xC0);
  SCB_SET_PRIORITY(SVCALL_SHPR, SVCALL_PRIORITY_POS, 0xD0);
  SCB_SET_PRIORITY(SYSTICK_SHPR, SYSTICK_PRIORITY_POS, 0xE0);
  SCB_SET_PRIORITY(PENDSV_SHPR, PENDSV_PRIORITY_POS, 0xF0);

  kprintf("[boot] ok, sysclk 84MHz, console=%s\r\n", "usart2");

  // create required tasks
  tcb_t *cli = create_task(0, cli_task, 0);
  cli->name = "CLI";
  cli->unprivileged = 0;
  tcb_t *idle_task = create_task(0xFF, idle_func, 0);
  idle_task->name = "IDLE";
  tcb_t *user_task = create_task(1, user_main, 0);
  user_task->name = "user";

  kprintf("[boot] Initializing tasks: %s, %s, %s\r\n", cli->name,
          idle_task->name, user_task->name);

  scheduler_put_task(user_task);
  scheduler_put_task(idle_task);
  scheduler_put_task(cli);

  systick_init();

  while (1) {
  }

  return 0;
}
