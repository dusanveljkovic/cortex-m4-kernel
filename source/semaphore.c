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
sem_result_t semaphore_wait(semaphore_t *sem) {
  uint32_t irq_state = irq_save();
  if (sem->state != SEM_OPEN) {
    irq_restore(irq_state);
    return SEM_CLOSE;
  }

  if (sem->count > 0) {
    irq_restore(irq_state);
    sem->count--;
    return SEM_OK;
  } else {
    tcb_t *task = current_task;

    task->state = TASK_BLOCKED;
    task->waiting_on = sem;
    task->waiting_on->references++;

    queue_push(&sem->wait_queue, task);
  }
  irq_restore(irq_state);
  return SEM_OK_YIELD;
}

sem_result_t semaphore_post(semaphore_t *sem) {
  uint32_t irq_state = irq_save();

  if (sem->state != SEM_OPEN) {
    irq_restore(irq_state);
    return SEM_CLOSE;
  }

  tcb_t *task = queue_pop(&sem->wait_queue);

  if (task != 0) {
    task->state = TASK_READY;
    task->waiting_on->references--;
    task->waiting_on = 0;

    scheduler_put_task(task);
  } else {
    sem->count++;
  }
  irq_restore(irq_state);
  return SEM_OK_YIELD;
}

void semaphore_close(semaphore_t *sem) {
  tcb_t *task = queue_pop(&sem->wait_queue);
  while (task) {
    scheduler_put_task(task);
    task = queue_pop(&sem->wait_queue);
  }
  sem->state = SEM_CLOSED;
}
