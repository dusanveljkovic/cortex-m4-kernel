#ifndef _TCB_H
#define _TCB_H

#include <stdint.h>

typedef enum {
  TASK_UNUSED = 0,
  TASK_READY,
  TASK_RUNNING,
  TASK_BLOCKED,
  TASK_SUSPENDED,
  TASK_FINISHED
} task_state_t;

typedef struct tcb {
  uint32_t *sp;
  task_state_t state;
  uint8_t priority;
  uint32_t *stack_top;
  uint32_t stack_size;
  struct tcb *next;
  const char *name;
  void (*function)(void *);
  void *args;

  void *waiting_on;
} tcb_t;

tcb_t *create_task(uint8_t priority, void (*func)(void *), void *args);

#endif // _TCB_H
