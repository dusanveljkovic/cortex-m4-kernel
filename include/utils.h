#ifndef _UTILS_H
#define _UTILS_H

#include <stdint.h>

int strcmp(const char *s1, const char *s2);
int strlen(const char *str);
void *memset(void *dst, int val, uint32_t len);
void *memcpy(void *dest, const void *src, uint32_t size);
int memcmp(const void *s1, const void *s2, uint32_t size);
uint32_t crc32(const uint8_t *data, uint32_t length);

#endif
