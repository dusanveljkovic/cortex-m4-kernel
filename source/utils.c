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

int memcmp(const void *s1, const void *s2, uint32_t size) {
  const uint8_t *p1 = (const uint8_t *)s1;
  const uint8_t *p2 = (const uint8_t *)s2;
  for (uint32_t i = 0; i < size; i++) {
    if (p1[i] != p2[i]) {
      return p1[i] - p2[i];
    }
  }
  return 0;
}

uint32_t crc32(const uint8_t *data, uint32_t length) {
  uint32_t crc = 0xFFFFFFFF;

  for (uint32_t i = 0; i < length; i++) {
    crc ^= data[i];

    for (int j = 0; j < 8; j++) {
      if (crc & 1) {
        crc = (crc >> 1) ^ 0xEDB88320;
      } else {
        crc >>= 1;
      }
    }
  }

  return ~crc;
}
