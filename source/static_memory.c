#include "../include/semaphore.h"
#include "../include/tcb.h"
#include "stdint.h"

#define N_TASKS 8
#define TASK_STACK_SIZE 256

#define N_SEMAPHORES 32
#define N_MUTEXES 32

static tcb_t task_slots[N_TASKS];
static uint32_t task_stacks[N_TASKS][TASK_STACK_SIZE];

static semaphore_t semaphore_slots[N_SEMAPHORES];
static mutex_t mutex_slots[N_MUTEXES];

void task_memory_init(void) {
  for (int i = 0; i < N_TASKS; i++) {
    task_slots[i].state = TASK_UNUSED;
    task_slots[i].sp = 0;
    task_slots[i].base_priority = 0;
    task_slots[i].effective_priority = 0;
    task_slots[i].waiting_on = 0;
    task_slots[i].next = 0;
  }
}

tcb_t *alloc_task(void) {
  for (int i = 0; i < N_TASKS; i++) {
    if (task_slots[i].state == TASK_UNUSED ||
        task_slots[i].state == TASK_FINISHED) {
      task_slots[i].state = TASK_READY;
      task_slots[i].stack_top = &task_stacks[i][127];
      return &task_slots[i];
    }
  }

  return 0;
}

void semaphore_memory_init(void) {
  for (int i = 0; i < N_SEMAPHORES; i++) {
    semaphore_slots[i].state = SEM_UNUSED;
    semaphore_slots[i].references = 0;
    semaphore_slots[i].wait_queue.head = 0;
    semaphore_slots[i].wait_queue.tail = 0;
  }
}

semaphore_t *alloc_semaphore(void) {
  for (int i = 0; i < N_SEMAPHORES; i++) {
    semaphore_t *sem = &semaphore_slots[i];
    if (sem->state == SEM_UNUSED ||
        (sem->state == SEM_CLOSED && sem->references == 0)) {
      sem->state = SEM_OPEN;
      return sem;
    }
  }

  return 0;
}

void mutex_memory_init(void) {
  for (int i = 0; i < N_MUTEXES; i++) {
    mutex_slots[i].state = SEM_UNUSED;
  }
}

mutex_t *alloc_mutex(void) {
  for (int i = 0; i < N_MUTEXES; i++) {
    mutex_t *m = &mutex_slots[i];
    if (m->state == SEM_UNUSED) {
      m->state = SEM_OPEN;
      return m;
    }
  }
  return 0;
}

void static_memory_init(void) {
  task_memory_init();
  semaphore_memory_init();
  mutex_memory_init();
}
