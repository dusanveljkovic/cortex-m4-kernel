#include "../include/utils.h"
#include "stdint.h"

int strcmp(const char *s1, const char *s2) {
  while (*s1 && (*s1 == *s2)) {
    s1++;
    s2++;
  }

  return (uint8_t)*s1 - (uint8_t)*s2;
}

int strlen(const char *str) {
  int count = 0;
  while (*str++ != '\0')
    count++;
  return count;
}

void *memset(void *dst, int val, uint32_t len) {
  uint8_t *ptr = (uint8_t *)dst;
  while (len-- > 0) {
    *ptr++ = (uint8_t)val;
  }
  return dst;
}

void *memcpy(void *dest, const void *src, uint32_t size) {
  uint8_t *d = (uint8_t *)dest;
  const uint8_t *s = (const uint8_t *)src;
  while (size--) {
    *d++ = *s++;
  }

  return dest;
}
