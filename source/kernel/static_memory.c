#include "../../include/arm.h"
#include "../../include/tcb.h"
#include "stdint.h"

#define N_TASKS 8
#define TASK_STACK_SIZE 1024

static tcb_t task_slots[N_TASKS];
__attribute__((aligned(
    TASK_STACK_SIZE))) static uint8_t task_stacks[N_TASKS][TASK_STACK_SIZE];

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

void static_memory_init(void) { task_memory_init(); }
