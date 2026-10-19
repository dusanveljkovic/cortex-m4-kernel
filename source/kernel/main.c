#include "../../include/cli.h"
#include "../../include/clock.h"
#include "../../include/console.h"
#include "../../include/fpu.h"
#include "../../include/heap.h"
#include "../../include/kprintf.h"
#include "../../include/mpu.h"
#include "../../include/net.h"
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

void fpu_init(void) {
  // enable coprocessor access
  CPACR |= (CPACR_CP_FULL_ACCESS << CPACR_CP_Pos(10));
  CPACR |= (CPACR_CP_FULL_ACCESS << CPACR_CP_Pos(11));
  __DSB();
  __ISB();

  // enable fpu state preservation and lazy stacking
  FPCCR |= (1 << FPCCR_LSPEN_Pos) | (1 << FPCCR_ASPEN_Pos);
}

int main() {
  static_memory_init();
  heap_init(&_heap_start, (uint32_t)(&_heap_end - &_heap_start));

  fpu_init();
  clock_init();
  driver_register(&usart2_driver);
  console_attach(driver_find("usart2"));

  spi1_init();

  scheduler_init();
  // mpu_init();

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
  tcb_t *net_t = create_task(0, net_task, 0);
  net_t->name = "net task";
  net_t->unprivileged = 0;

  kprintf("[boot] Initializing tasks: %s, %s, %s, %s\r\n", cli->name,
          idle_task->name, user_task->name, net_t->name);

  scheduler_put_task(user_task);
  scheduler_put_task(idle_task);
  scheduler_put_task(cli);
  scheduler_put_task(net_t);

  systick_init();

  while (1) {
  }

  return 0;
}
