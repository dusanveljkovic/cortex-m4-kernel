#ifndef _STATIC_MEMORY_H
#define _STATIC_MEMORY_H
#include "semaphore.h"
#include "tcb.h"

void static_memory_init(void);

tcb_t *alloc_task(void);

#endif // !_STATIC_MEMORY_H
