#ifndef _SEMAPHORE_H
#define _SEMAPHORE_H

#include "scheduler.h"

typedef struct semaphore {
  int count;

  tcb_list_t wait_queue;
} semaphore_t;

void semaphore_init(semaphore_t *sem, int count);
void semaphore_wait(semaphore_t *sem);

#endif // !_SEMAPHORE_H
