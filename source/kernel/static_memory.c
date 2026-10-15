#include "../../include/static_memory.h"
#include "../../include/arm.h"
#include "../../include/tcb.h"
#include "stdint.h"
#include <semaphore.h>

#define N_SEMAPHORES 64
#define N_MUTEXES 64
static semaphore_t sem_slots[N_SEMAPHORES];
static mutex_t mutex_slots[N_MUTEXES];

static tcb_t task_slots[N_TASKS];
__attribute__((aligned(
    TASK_STACK_SIZE))) static uint8_t task_stacks[N_TASKS][TASK_STACK_SIZE];

static void task_memory_init(void) {
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
  uint32_t irq_state = irq_save();

  for (int i = 0; i < N_TASKS; i++) {
    if (task_slots[i].state == TASK_UNUSED ||
        task_slots[i].state == TASK_FINISHED) {
      task_slots[i].state = TASK_READY;
      task_slots[i].stack_base = &task_stacks[i][0];
      task_slots[i].stack_size = TASK_STACK_SIZE;
      irq_restore(irq_state);
      return &task_slots[i];
    }
  }

  irq_restore(irq_state);
  return 0;
}

tcb_t *get_task(int idx) {
  if (idx >= N_TASKS || idx < 0)
    return 0;
  return &task_slots[idx];
}

static void semaphore_memory_init(void) {
  for (int i = 0; i < N_SEMAPHORES; i++) {
    sem_slots[i].count = 0;
    sem_slots[i].references = 0;
    sem_slots[i].state = SEM_UNUSED;
  }
}

semaphore_t *alloc_sem(void) {
  uint32_t irq_state = irq_save();

  for (int i = 0; i < N_SEMAPHORES; i++) {
    if (sem_slots[i].state == SEM_UNUSED) {
      sem_slots[i].state = SEM_OPEN;
      irq_restore(irq_state);
      return &sem_slots[i];
    }
  }

  irq_restore(irq_state);
  return 0;
}

void free_sem(semaphore_t *sem) {
  uint32_t irq_state = irq_save();
  sem->state = SEM_UNUSED;
  irq_restore(irq_state);
}

static void mutex_memory_init(void) {
  for (int i = 0; i < N_MUTEXES; i++) {
    mutex_slots[i].owner = 0;
    mutex_slots[i].state = SEM_UNUSED;
    mutex_slots[i].next_owned = 0;
  }
}

mutex_t *alloc_mutex(void) {
  uint32_t irq_state = irq_save();

  for (int i = 0; i < N_MUTEXES; i++) {
    if (mutex_slots[i].state == SEM_UNUSED) {
      mutex_slots[i].state = SEM_OPEN;
      irq_restore(irq_state);
      return &mutex_slots[i];
    }
  }

  irq_restore(irq_state);
  return 0;
}

bool validate_task(uintptr_t ptr) {
  return (((uintptr_t)ptr - (uintptr_t)&task_slots[0]) % sizeof(tcb_t) == 0);
}
bool validate_sem(uintptr_t ptr) {
  return (((uintptr_t)ptr - (uintptr_t)&sem_slots[0]) % sizeof(semaphore_t) ==
          0);
}
bool validate_mutex(uintptr_t ptr) {
  return (((uintptr_t)ptr - (uintptr_t)&mutex_slots[0]) % sizeof(mutex_t) == 0);
}

void static_memory_init(void) {
  task_memory_init();
  semaphore_memory_init();
  mutex_memory_init();
}
