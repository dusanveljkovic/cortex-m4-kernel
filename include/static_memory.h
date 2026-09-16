#ifndef _STATIC_MEMORY_H
#define _STATIC_MEMORY_H
#include "semaphore.h"
#include "tcb.h"

void static_memory_init(void);

tcb_t *alloc_task(void);
semaphore_t *alloc_semaphore(void);
mutex_t *alloc_mutex(void);

#endif // !_STATIC_MEMORY_H
