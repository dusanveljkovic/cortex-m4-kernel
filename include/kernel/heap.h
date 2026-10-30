#ifndef _HEAP_H
#define _HEAP_H

#include "stdint.h"

void heap_init(void *heap_start, uint32_t heap_size);
void *kmalloc(uint32_t size);
void *kcalloc(uint32_t count, uint32_t size);
void kfree(void *ptr);
uint32_t heap_free_size(void);

#endif // !_HEAP_H
