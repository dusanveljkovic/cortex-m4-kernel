#include "../../include/kprintf.h"
#include "../../include/console.h"
#include "../../include/syscall.h"
#include "../../include/utils.h"
#include "stdbool.h"

#define KPRINTF_BUF_SIZE 256

typedef struct {
  char *buf;
  uint32_t cap;
  uint32_t len; // counts every char wanted, even past cap (snprintf style)
} out_t;

static void emit(out_t *o, char c) {
  if (o->cap != 0 && o->len < o->cap - 1)
    o->buf[o->len] = c;
  o->len++;
}

static void emit_number(out_t *o, uint32_t v, bool neg, uint32_t base,
                        bool upper, uint32_t width, char pad) {
  static const char lower_digits[] = "0123456789abcdef";
  static const char upper_digits[] = "0123456789ABCDEF";
  const char *digits = upper ? upper_digits : lower_digits;

  char tmp[10]; // 32-bit max: 10 decimal digits, 8 hex digits
  uint32_t n = 0;
  do {
    tmp[n++] = digits[v % base];
    v /= base;
  } while (v != 0);

  uint32_t total = n + (neg ? 1 : 0);
  if (pad == ' ') {
    for (uint32_t i = total; i < width; i++)
      emit(o, ' ');
    if (neg)
      emit(o, '-');
  } else {
    if (neg)
      emit(o, '-');
    for (uint32_t i = total; i < width; i++)
      emit(o, '0');
  }
  while (n != 0)
    emit(o, tmp[--n]);
}

int kvsnprintf(char *buf, uint32_t cap, const char *fmt, va_list ap) {
  out_t o = {buf, cap, 0};

  for (; *fmt; fmt++) {
    if (*fmt != '%') {
      emit(&o, *fmt);
      continue;
    }
    fmt++;

    char pad = ' ';
    uint32_t width = 0;
    if (*fmt == '0') {
      pad = '0';
      fmt++;
    }
    while (*fmt >= '0' && *fmt <= '9') {
      width = width * 10 + (uint32_t)(*fmt - '0');
      fmt++;
    }

    switch (*fmt) {
    case 'c':
      emit(&o, (char)va_arg(ap, int));
      break;
    case 's': {
      const char *s = va_arg(ap, const char *);
      uint32_t len = strlen(s);
      if (s == 0) {
        s = "(null)";
        len = 6;
      }
      int str_pad = width - len;
      while (str_pad > 0) {
        emit(&o, ' ');
        str_pad--;
      }
      while (*s)
        emit(&o, *s++);
      break;
    }
    case 'd':
    case 'i': {
      int v = va_arg(ap, int);
      bool neg = v < 0;
      emit_number(&o, neg ? (uint32_t)0 - (uint32_t)v : (uint32_t)v, neg, 10,
                  false, width, pad);
      break;
    }
    case 'u':
      emit_number(&o, va_arg(ap, unsigned int), false, 10, false, width, pad);
      break;
    case 'x':
      emit_number(&o, va_arg(ap, unsigned int), false, 16, false, width, pad);
      break;
    case 'X':
      emit_number(&o, va_arg(ap, unsigned int), false, 16, true, width, pad);
      break;
    case 'p':
      emit(&o, '0');
      emit(&o, 'x');
      emit_number(&o, (uint32_t)(uintptr_t)va_arg(ap, void *), false, 16, false,
                  8, '0');
      break;
    case '%':
      emit(&o, '%');
      break;
    case '\0': // format ended with a lone '%': print it, then stop
      emit(&o, '%');
      goto done;
    default: // unknown specifier: print it verbatim so it's noticeable
      emit(&o, '%');
      emit(&o, *fmt);
      break;
    }
  }

done:
  if (cap != 0)
    buf[o.len < cap ? o.len : cap - 1] = '\0';
  return (int)o.len;
}

int ksnprintf(char *buf, uint32_t cap, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int n = kvsnprintf(buf, cap, fmt, ap);
  va_end(ap);
  return n;
}

static uint32_t clamp_len(int n, uint32_t cap) {
  if (n < 0)
    return 0;
  return (uint32_t)n >= cap ? cap - 1 : (uint32_t)n;
}

int kprintf(const char *fmt, ...) {
  char buf[KPRINTF_BUF_SIZE];
  va_list ap;
  va_start(ap, fmt);
  int n = kvsnprintf(buf, sizeof buf, fmt, ap);
  va_end(ap);

  console_write(buf, clamp_len(n, sizeof buf));
  return n;
}

int kpanicf(const char *fmt, ...) {
  char buf[KPRINTF_BUF_SIZE];
  va_list ap;
  va_start(ap, fmt);
  int n = kvsnprintf(buf, sizeof buf, fmt, ap);
  va_end(ap);

  console_write_sync(buf, clamp_len(n, sizeof buf));
  return n;
}
