#ifndef _SEMAPHORE_H
#define _SEMAPHORE_H

#include "scheduler.h"

typedef enum { SEM_UNUSED = 0, SEM_OPEN, SEM_CLOSED } sem_state_t;

typedef enum {
  SEM_CLOSE,
  SEM_OK,
  SEM_OK_YIELD,
  MUTEX_NOT_OWNER,
  MUTEX_DEADLOCK
} sem_result_t;

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

typedef struct mutex {
  tcb_t *owner;
  tcb_list_t wait_queue;

  sem_state_t state;
} mutex_t;

void mutex_init(mutex_t *m);
sem_result_t mutex_lock(mutex_t *m);
sem_result_t mutex_unlock(mutex_t *m);
void mutex_close(mutex_t *m);

#endif // !_SEMAPHORE_H
