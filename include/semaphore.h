#ifndef _SEMAPHORE_H
#define _SEMAPHORE_H

#include "scheduler.h"

typedef enum { SEM_UNUSED = 0, SEM_OPEN, SEM_CLOSED } sem_state_t;

typedef struct semaphore {
  uint32_t count;

  tcb_list_t wait_queue;

  sem_state_t state;
} semaphore_t;

void semaphore_init(semaphore_t *sem, int count);
uint8_t semaphore_wait(semaphore_t *sem);
void semaphore_post(semaphore_t *sem);

#endif // !_SEMAPHORE_H
