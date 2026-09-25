#include "../include/utils.h"
#include "../include/usart.h"
#include "stdarg.h"
#include "stdint.h"

#define putchar usart2_putc
#define putchar_fault usart2_fault_putc

int strcmp(const char *s1, const char *s2) {
  while (*s1 && (*s1 == *s2)) {
    s1++;
    s2++;
  }

  return (uint8_t)*s1 - (uint8_t)*s2;
}

static void print_string(const char *s, void (*putcf)(char)) {
  if (!s)
    s = "(null)";

  while (*s)
    putcf(*s++);
}

static void print_unsigned(uint32_t value, void (*putcf)(char)) {
  char buffer[11];
  int i = 0;

  if (value == 0) {
    putcf('0');
    return;
  }

  while (value > 0) {
    buffer[i++] = '0' + (value % 10);
    value /= 10;
  }

  while (i--)
    putcf(buffer[i]);
}

static void print_signed(int32_t value, void (*putcf)(char)) {
  if (value < 0) {
    putcf('-');
    value = -value;
  }

  print_unsigned((uint32_t)value, putcf);
}

static void print_hex(uint32_t value, void (*putcf)(char)) {
  static const char hex[] = "0123456789abcdef";

  putcf('0');
  putcf('x');

  for (int i = 7; i >= 0; i--) {
    uint32_t digit = (value >> (i * 4)) & 0xF;
    putcf(hex[digit]);
  }
}

int printf(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);

  while (*fmt) {

    if (*fmt != '%') {
      putchar(*fmt++);
      continue;
    }

    fmt++;

    switch (*fmt) {

    case 'c':
      putchar((char)va_arg(args, int));
      break;

    case 's':
      print_string(va_arg(args, char *), putchar);
      break;

    case 'd':
      print_signed(va_arg(args, int32_t), putchar);
      break;

    case 'u':
      print_unsigned(va_arg(args, uint32_t), putchar);
      break;

    case 'x':
      print_hex(va_arg(args, uint32_t), putchar);
      break;

    case 'p':
      print_hex((uint32_t)va_arg(args, void *), putchar);
      break;

    case '%':
      putchar('%');
      break;

    default:
      putchar('%');
      putchar(*fmt);
      break;
    }

    fmt++;
  }

  va_end(args);
  return 0;
}

int printf_fault(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);

  while (*fmt) {

    if (*fmt != '%') {
      putchar_fault(*fmt++);
      continue;
    }

    fmt++;

    switch (*fmt) {

    case 'c':
      putchar_fault((char)va_arg(args, int));
      break;

    case 's':
      print_string(va_arg(args, char *), putchar_fault);
      break;

    case 'd':
      print_signed(va_arg(args, int32_t), putchar_fault);
      break;

    case 'u':
      print_unsigned(va_arg(args, uint32_t), putchar_fault);
      break;

    case 'x':
      print_hex(va_arg(args, uint32_t), putchar_fault);
      break;

    case 'p':
      print_hex((uint32_t)va_arg(args, void *), putchar_fault);
      break;

    case '%':
      putchar_fault('%');
      break;

    default:
      putchar_fault('%');
      putchar_fault(*fmt);
      break;
    }

    fmt++;
  }

  va_end(args);
  return 0;
}
