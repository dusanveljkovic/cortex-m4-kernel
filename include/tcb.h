#ifndef _TCB_H
#define _TCB_H

#include <stdint.h>

typedef enum {
  TASK_UNUSED = 0,
  TASK_READY,
  TASK_RUNNING,
  TASK_BLOCKED,
  TASK_SUSPENDED,
  TASK_SLEEPING,
  TASK_FINISHED
} task_state_t;

typedef struct tcb {
  uint32_t *sp;
  task_state_t state;

  uint8_t base_priority;
  uint8_t effective_priority;

  uint8_t *stack_base;
  uint32_t stack_size;
  struct tcb *next;
  const char *name;
  void (*function)(void *);
  void *args;

  uint8_t unprivileged;
  uint32_t wake_tick;

  void *waiting_on;

  struct mutex *owned_mutexes;
} tcb_t;

tcb_t *create_task(uint8_t priority, void (*func)(void *), void *args);
uint8_t task_recalculate_priority(tcb_t *task);

#endif // _TCB_H
