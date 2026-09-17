#include "../../include/heap.h"
#include "stdint.h"

#define HEAP_ALIGMENT 8
#define align(size) (size + HEAP_ALIGMENT - 1) & ~(HEAP_ALIGMENT - 1)

typedef struct heap_block {
  uint32_t size;
  struct heap_block *next;
  struct heap_block *prev;
  uint8_t free;
} heap_block_t;

static heap_block_t *heap_head;

void heap_init(void *heap_start, uint32_t heap_size) {
  heap_head = (heap_block_t *)heap_start;

  heap_head->size = heap_size - sizeof(heap_block_t);
  heap_head->next = 0;
  heap_head->prev = 0;
  heap_head->free = 1;
}

void *kmalloc(uint32_t size) {
  if (size == 0)
    return 0;

  size = align(size);

  heap_block_t *tmp = heap_head;
  while (tmp != 0) {
    if (tmp->free && tmp->size >= size) {
      if (tmp->size >= size + sizeof(heap_block_t) + HEAP_ALIGMENT) {
        heap_block_t *new_block = (heap_block_t *)((uint8_t *)(tmp + 1) + size);
        new_block->size = tmp->size - size - sizeof(heap_block_t);

        new_block->next = tmp->next;
        new_block->free = 1;

        tmp->size = size;
        tmp->next = new_block;
        tmp->prev = new_block->prev;

        new_block->prev = tmp;
      }

      tmp->free = 0;
      return (void *)(tmp + 1);
    }
    tmp = tmp->next;
  }

  return 0;
}

void *kcalloc(uint32_t count, uint32_t size) {
  if (count != 0 && size > SIZE_MAX / count)
    return 0;

  uint32_t total = count * size;

  void *ptr = kmalloc(total);

  // if (ptr != 0)
  //   memset(ptr, 0, total);

  return ptr;
}

void kfree(void *ptr) {
  if (ptr == 0)
    return;

  heap_block_t *block = (heap_block_t *)ptr - 1;

  block->free = 1;

  if (block->next != 0 && block->next->free) {
    heap_block_t *next = block->next;

    block->size += sizeof(heap_block_t) + next->size;

    block->next = next->next;
    block->next->prev = block;
  }

  if (block->prev != 0 && block->prev->free) {
    heap_block_t *prev = block->prev;
    prev->size += sizeof(heap_block_t) + block->size;

    prev->next = block->next;
    prev->next->prev = prev;
  }
}
