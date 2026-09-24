#include "../../include/semaphore.h"
#include "../../include/arm.h"
#include "../../include/scheduler.h"

void semaphore_init(semaphore_t *sem, int count) {
  sem->state = SEM_OPEN;
  sem->count = count;
  sem->wait_queue.head = 0;
  sem->wait_queue.tail = 0;
}

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
    ((semaphore_t *)task->waiting_on)->references++;

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
    ((semaphore_t *)task->waiting_on)->references--;
    task->waiting_on = 0;

    scheduler_put_task(task);
  } else {
    sem->count++;
  }
  irq_restore(irq_state);
  return SEM_OK_YIELD;
}

void semaphore_close(semaphore_t *sem) {
  uint32_t irq_state = irq_save();

  tcb_t *task = queue_pop(&sem->wait_queue);
  while (task) {
    task->waiting_on = 0;
    scheduler_put_task(task);
    task = queue_pop(&sem->wait_queue);
  }
  sem->state = SEM_CLOSED;

  irq_restore(irq_state);
}

void mutex_init(mutex_t *m) {
  m->state = SEM_OPEN;
  m->owner = 0;
  m->wait_queue.head = 0;
  m->wait_queue.tail = 0;
  m->next_owned = 0;
}

static void mutex_queue_push(mutex_t **head, mutex_t *m) {
  m->next_owned = *head;
  *head = m;
}

static void mutex_queue_remove(mutex_t **head, mutex_t *m) {
  if (m == *head) {
    *head = m->next_owned;
    m->next_owned = 0;
    return;
  }
  mutex_t *tmp = *head;
  mutex_t *prev = 0;
  while (tmp != 0) {
    if (tmp == m) {
      prev->next_owned = tmp->next_owned;
      tmp->next_owned = 0;
      return;
    }
    prev = tmp;
    tmp = tmp->next_owned;
  }
}

sem_result_t mutex_lock(mutex_t *m) {
  uint32_t irq_state = irq_save();
  if (m->owner == 0) {
    m->owner = current_task;
    mutex_queue_push(&current_task->owned_mutexes, m);
    irq_restore(irq_state);
    return SEM_OK;
  }
  if (m->owner == current_task) {
    irq_restore(irq_state);
    return MUTEX_DEADLOCK;
  }

  tcb_t *task = current_task;

  task->state = TASK_BLOCKED;
  task->waiting_on = m;

  priority_queue_push(&m->wait_queue, task);

  if (task->effective_priority < m->owner->effective_priority) {
    m->owner->effective_priority = task->effective_priority;
    if (m->owner->state == TASK_READY)
      scheduler_reorder(m->owner);
  }

  irq_restore(irq_state);
  return SEM_OK_YIELD;
}

sem_result_t mutex_unlock(mutex_t *m) {
  uint32_t irq_state = irq_save();
  if (m->owner != current_task) {
    irq_restore(irq_state);
    return MUTEX_NOT_OWNER;
  }
  tcb_t *next = priority_queue_pop(&m->wait_queue);

  m->owner = next;
  if (next != 0) {
    next->state = TASK_READY;
    next->waiting_on = 0;

    mutex_queue_push(&next->owned_mutexes, m);
    next->effective_priority = task_recalculate_priority(next);
    scheduler_put_task(next);
  }
  mutex_queue_remove(&current_task->owned_mutexes, m);

  uint8_t new_priority = task_recalculate_priority(current_task);
  current_task->effective_priority = new_priority;

  if (next != 0 &&
      next->effective_priority < current_task->effective_priority) {
    return SEM_OK_YIELD;
    irq_restore(irq_state);
  }

  irq_restore(irq_state);
  return SEM_OK;
}
