#include "../include/nvic.h"
#include "../include/scb.h"
#include "../include/scheduler.h"
#include "../include/semaphore.h"
#include "../include/static_memory.h"
#include "../include/syscall.h"
#include "../include/systick.h"
#include <stdint.h>

static uint32_t data = 0;
extern uint32_t *task_stack_init(uint32_t *stack_top, void (*func)(void));

typedef struct {
  int number;
  semaphore_t *sem1;
  semaphore_t *sem2;
} wrapper_t;

void add_task(void *arg) {
  wrapper_t w = *(wrapper_t *)arg;
  while (1) {
    data += w.number;
    sys_sem_post(w.sem1);
    sys_sem_wait(w.sem2);
  }
}

void sub_task(void *arg) {
  wrapper_t w = *(wrapper_t *)arg;
  while (1) {
    data -= w.number;
    sys_sem_post(w.sem2);
    sys_sem_wait(w.sem1);
  }
}

uint32_t main() {
  data = 0;
  static_memory_init();
  scheduler_init();

  SCB->SHPR2 = 0xE0000000;
  SCB->SHPR3 = 0xE0F00000;
  semaphore_t *sem1 = sys_sem_create(0);
  semaphore_t *sem2 = sys_sem_create(0);
  wrapper_t arg1 = {1, sem1, sem2};
  wrapper_t arg2 = {1, sem1, sem2};
  tcb_t *t1 = sys_task_create(0, add_task, &arg1);
  tcb_t *t2 = sys_task_create(1, sub_task, &arg2);
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
