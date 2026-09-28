#ifndef _KPRINTF_H
#define _KPRINTF_H

#include "stdarg.h"
#include "stdint.h"

int kvsnprintf(char *buf, uint32_t cap, const char *fmt, va_list ap);
int ksnprintf(char *buf, uint32_t cap, const char *fmt, ...);

// kernel side printf
int kprintf(const char *fmt, ...);

// kernel side printf but uses polling
int kpanicf(const char *fmt, ...);

#endif
