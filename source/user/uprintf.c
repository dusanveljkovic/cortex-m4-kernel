#include "uprintf.h"
#include "../../include/kprintf.h"
#include "../../include/syscall.h"

static uint32_t clamp_len(int n, uint32_t cap) {
  if (n < 0)
    return 0;
  return (uint32_t)n >= cap ? cap - 1 : (uint32_t)n;
}

int uprintf(const char *fmt, ...) {
  char buf[128]; // lives on the task's own (small) stack
  va_list ap;
  va_start(ap, fmt);
  int n = kvsnprintf(buf, sizeof buf, fmt, ap);
  va_end(ap);

  sys_console_write(buf, clamp_len(n, sizeof buf));
  return n;
}
