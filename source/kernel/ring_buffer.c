#include "../../include/ring_buffer.h"

void ring_buffer_put(ring_buffer_t *rb, uint8_t data) {
  rb->buffer[rb->head] = data;
  rb->head++;
  if (rb->head == rb->size) {
    rb->head = 0;
  }
}
uint8_t ring_buffer_get(ring_buffer_t *rb) {
  uint8_t data = rb->buffer[rb->tail];
  rb->tail++;
  if (rb->tail == rb->size) {
    rb->tail = 0;
  }
  return data;
}
uint8_t ring_buffer_peek(ring_buffer_t *rb) { return rb->buffer[rb->tail]; }
bool ring_buffer_empty(ring_buffer_t *rb) { return rb->head == rb->tail; }
bool ring_buffer_full(ring_buffer_t *rb) {
  uint8_t idx_after_head = rb->head + 1;
  if (idx_after_head == rb->size) {
    idx_after_head = 0;
  }
  return idx_after_head == rb->tail;
}
