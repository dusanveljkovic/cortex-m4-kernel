#ifndef _STATIC_MEMORY_H
#define _STATIC_MEMORY_H
#include "semaphore.h"
#include "stdbool.h"
#include "tcb.h"

#define N_TASKS 16
#define TASK_STACK_SIZE 1024

void static_memory_init(void);

tcb_t *alloc_task(void);
semaphore_t *alloc_sem(void);
mutex_t *alloc_mutex(void);

void free_sem(semaphore_t *sem);

bool validate_task(uintptr_t ptr);
bool validate_sem(uintptr_t ptr);
bool validate_mutex(uintptr_t ptr);

tcb_t *get_task(int idx);

#endif // !_STATIC_MEMORY_H
