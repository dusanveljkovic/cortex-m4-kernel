#include "../include/semaphore.h"
#include "../include/scheduler.h"

void semaphore_init(semaphore_t *sem, int count) {
  sem->count = count;
  sem->wait_queue.head = 0;
  sem->wait_queue.tail = 0;
}

// current thread waits on semaphore
// used by kernel
// return 1 if current thread should yield
int semaphore_wait(semaphore_t *sem) {
  disable_interrupts();
  uint8_t should_yield = 0;
  if (sem->count > 0) {
    sem->count--;
  } else {
    tcb_t *task = current_task;

    task->state = TASK_BLOCKED;
    task->waiting_on = sem;

    queue_push(&sem->wait_queue, task);

    should_yield = 1;
  }
  enable_interrupts();
  return should_yield;
}
