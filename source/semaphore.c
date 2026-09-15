#include "../include/semaphore.h"
#include "../include/arm.h"
#include "../include/scheduler.h"

void semaphore_init(semaphore_t *sem, int count) {
  sem->count = count;
  sem->wait_queue.head = 0;
  sem->wait_queue.tail = 0;
}

// current thread waits on semaphore
// used by kernel
// return 1 if current thread should yield
uint8_t semaphore_wait(semaphore_t *sem) {
  uint32_t irq_state = irq_save();
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
  irq_restore(irq_state);
  return should_yield;
}

void semaphore_post(semaphore_t *sem) {
  uint32_t irq_state = irq_save();

  tcb_t *task = queue_pop(&sem->wait_queue);

  if (task != 0) {
    task->state = TASK_READY;
    task->waiting_on = 0;

    scheduler_put_task(task);
  } else {
    sem->count++;
  }
  irq_restore(irq_state);
}
