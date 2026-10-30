#ifndef _RING_BUFFER_H
#define _RING_BUFFER_H

#include "stdbool.h"
#include "stdint.h"

typedef struct {
  uint8_t *buffer;
  uint8_t size;
  uint8_t head;
  uint8_t tail;
} ring_buffer_t;

void ring_buffer_put(ring_buffer_t *rb, uint8_t data);
uint8_t ring_buffer_get(ring_buffer_t *rb);
uint8_t ring_buffer_peek(ring_buffer_t *rb);
bool ring_buffer_empty(ring_buffer_t *rb);
bool ring_buffer_full(ring_buffer_t *rb);

#endif
