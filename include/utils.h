#ifndef _UTILS_H
#define _UTILS_H

#include <stdint.h>

int strcmp(const char *s1, const char *s2);
int strlen(const char *str);
void *memset(void *dst, int val, uint32_t len);
void *memcpy(void *dest, const void *src, uint32_t size);

#endif
