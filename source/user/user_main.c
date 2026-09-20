#include "../../include/semaphore.h"
#include "../../include/syscall.h"

static uint32_t data = 0;

typedef struct {
  int number;
  semaphore_t *sem1;
  semaphore_t *sem2;
  mutex_t *m;
} wrapper_t;

static wrapper_t arg1;
static wrapper_t arg2;

void print(const char *str) {
  while (*str) {
    sys_putc(*str++);
  }
}

void high_task(void *arg) {
  wrapper_t w = *(wrapper_t *)arg;
  int g = 2;
  int runs = 10;
  while (1) {
    // sys_mutex_lock(w.m);
    // print("high task mutex lock\r\n");
    // data += w.number;
    // g -= 1;
    // if (g <= 0) {
    //   g = 2;
    //   print("high task mutex unlock\r\n");
    //   sys_mutex_unlock(w.m);
    // }
    // sys_yield();
    // sys_sem_post(w.sem1);
    // sys_sem_wait(w.sem2);
    sys_putc('H');
  }
}

void med_task(void *arg) {
  wrapper_t w = *(wrapper_t *)arg;
  int g = 2;
  int runs = 10;
  while (1) {
    // print("medium task processing\r\n");
    // data -= w.number;
    // g -= 1;
    // if (g <= 0) {
    //   g = 2;
    // }
    // sys_yield();
    // // sys_sem_post(w.sem2);
    // // sys_sem_wait(w.sem1);
    sys_putc('M');
  }
}

void low_task(void *arg) {
  tcb_t *t1 = sys_task_create(1, high_task, arg);
  tcb_t *t2 = sys_task_create(1, med_task, arg);
  // tcb_t *t3 = sys_task_create(2, med_task, arg);

  wrapper_t w = *(wrapper_t *)arg;
  int g = 2;
  int runs = 10;
  while (1) {
    // sys_mutex_lock(w.m);
    // print("low task mutex lock\r\n");
    // data -= w.number;
    // g -= 1;
    // if (g <= 0) {
    //   g = 2;
    //   print("low task mutex unlock\r\n");
    //   sys_mutex_unlock(w.m);
    // }
    // sys_yield();
    // // sys_sem_post(w.sem2);
    // // sys_sem_wait(w.sem1);
    sys_putc('L');
  }
}

void user_main(void *arg) {
  data = 0;
  semaphore_t *sem1 = sys_sem_create(0);
  semaphore_t *sem2 = sys_sem_create(0);
  mutex_t *m1 = sys_mutex_create();
  wrapper_t *parg1 = sys_malloc(sizeof(wrapper_t));
  parg1->number = 1;
  parg1->sem1 = sem1;
  parg1->sem2 = sem2;
  parg1->m = m1;
  arg1 = (wrapper_t){1, sem1, sem2, m1};
  arg2 = (wrapper_t){1, sem1, sem2, m1};
  tcb_t *t3 = sys_task_create(1, low_task, parg1);
  print("created all 3 tasks\r\n");
}
