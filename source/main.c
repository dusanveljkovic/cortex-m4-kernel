#include "../include/nvic.h"
#include "../include/scb.h"
#include "../include/scheduler.h"
#include "../include/semaphore.h"
#include "../include/static_memory.h"
#include "../include/syscall.h"
#include "../include/systick.h"
#include <stdint.h>

static uint32_t data = 0;

typedef struct {
  int number;
  semaphore_t *sem1;
  semaphore_t *sem2;
  mutex_t *m;
} wrapper_t;

static wrapper_t arg1;
static wrapper_t arg2;

void high_task(void *arg) {
  wrapper_t w = *(wrapper_t *)arg;
  int g = 2;
  while (1) {
    sys_mutex_lock(w.m);
    data += w.number;
    g -= 1;
    if (g <= 0) {
      g = 2;
      sys_mutex_unlock(w.m);
    }
    sys_yield();
    // sys_sem_post(w.sem1);
    // sys_sem_wait(w.sem2);
  }
}

void med_task(void *arg) {
  wrapper_t w = *(wrapper_t *)arg;
  int g = 2;
  while (1) {
    data -= w.number;
    g -= 1;
    if (g <= 0) {
      g = 2;
    }
    sys_yield();
    // sys_sem_post(w.sem2);
    // sys_sem_wait(w.sem1);
  }
}

void low_task(void *arg) {
  tcb_t *t1 = sys_task_create(1, high_task, &arg1);
  tcb_t *t2 = sys_task_create(2, med_task, &arg2);
  tcb_t *t3 = sys_task_create(2, med_task, &arg2);
  scheduler_put_task(t1);
  scheduler_put_task(t2);
  scheduler_put_task(t3);

  wrapper_t w = *(wrapper_t *)arg;
  int g = 2;
  while (1) {
    sys_mutex_lock(w.m);
    data -= w.number;
    g -= 1;
    if (g <= 0) {
      g = 2;
      sys_mutex_unlock(w.m);
    }
    sys_yield();
    // sys_sem_post(w.sem2);
    // sys_sem_wait(w.sem1);
  }
}

void user_main(void *arg) {
  data = 0;
  semaphore_t *sem1 = sys_sem_create(0);
  semaphore_t *sem2 = sys_sem_create(0);
  mutex_t *m1 = sys_mutex_create();
  arg1 = (wrapper_t){1, sem1, sem2, m1};
  arg2 = (wrapper_t){1, sem1, sem2, m1};
  tcb_t *t3 = sys_task_create(3, low_task, &arg2);
  scheduler_put_task(t3);
}

uint32_t main() {
  static_memory_init();
  scheduler_init();

  SCB->SHPR2 = 0xE0000000;
  SCB->SHPR3 = 0xE0F00000;
  tcb_t *user_task = sys_task_create(0, user_main, 0);
  scheduler_put_task(user_task);

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
