#include "../include/nvic.h"
#include "../include/scb.h"
#include "../include/scheduler.h"
#include "../include/static_memory.h"
#include "../include/syscall.h"
#include "../include/systick.h"
#include <stdint.h>

static uint32_t data = 0;
extern uint32_t *task_stack_init(uint32_t *stack_top, void (*func)(void));

void add_task(void *arg) {
  int number = *(int *)arg;
  data += number;
}

void kernel_main(void) {
  while (1) {
  }
}

uint32_t main() {
  static_memory_init();
  scheduler_init();

  SCB->SHPR2 = 0xE0000000;
  SCB->SHPR3 = 0xE0F00000;
  int n1 = 1, n2 = 2;
  tcb_t *t1 = sys_task_create(0, add_task, &n1);
  tcb_t *t2 = sys_task_create(1, add_task, &n2);
  scheduler_put_task(t1);
  scheduler_put_task(t2);

  // static tcb_t kernel;
  //
  // uint32_t *curr_sp;
  // asm volatile("mov %0, sp" : "=r"(curr_sp));
  // kernel.sp = task_stack_init(curr_sp, kernel_main);
  // current_task = &kernel;
  // asm volatile("msr psp, %0" ::"r"(kernel.sp));
  // asm volatile("mrs r1, control\n"
  //              "orr r1, r1, #2\n"
  //              "msr control, r1\n"
  //              "isb");

  sys_yield();

  // systick_init();

  while (1) {
  }

  return 0;
}
