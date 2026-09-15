#ifndef _SEMAPHORE_H
#define _SEMAPHORE_H

#include "scheduler.h"

typedef enum { SEM_UNUSED = 0, SEM_OPEN, SEM_CLOSED } sem_state_t;

typedef enum { SEM_CLOSE, SEM_OK, SEM_OK_YIELD } sem_result_t;

typedef struct semaphore {
  uint32_t count;

  tcb_list_t wait_queue;

  uint32_t references;

  sem_state_t state;
} semaphore_t;

void semaphore_init(semaphore_t *sem, int count);
sem_result_t semaphore_wait(semaphore_t *sem);
sem_result_t semaphore_post(semaphore_t *sem);
void semaphore_close(semaphore_t *sem);

#endif // !_SEMAPHORE_H
