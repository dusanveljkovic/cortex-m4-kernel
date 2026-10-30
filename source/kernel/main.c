#include "../../include/drivers/spi.h"
#include "../../include/drivers/usart.h"
#include "../../include/kernel/cli.h"
#include "../../include/kernel/clock.h"
#include "../../include/kernel/console.h"
#include "../../include/kernel/flash.h"
#include "../../include/kernel/fpu.h"
#include "../../include/kernel/heap.h"
#include "../../include/kernel/kprintf.h"
#include "../../include/kernel/mpu.h"
#include "../../include/kernel/net.h"
#include "../../include/kernel/nvic.h"
#include "../../include/kernel/scb.h"
#include "../../include/kernel/scheduler.h"
#include "../../include/kernel/static_memory.h"
#include "../../include/kernel/systick.h"
#include "../../include/kernel/tcb.h"

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

  spi_return_t spi_ret = spi1_configure();
  if (spi_ret != SPI_OK) {
    kprintf("[!][boot] Couldn't configure SPI1\r\n");
  } else {
    spi_ret = spi1_start();
    if (spi_ret != SPI_OK) {
      kprintf("[!][boot] Couldn't start SPI1\r\n");
    }
  }

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
