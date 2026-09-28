#include "../../include/driver.h"
#include "usart.h"

static int usart2_drv_init(void) {
  usart2_init();
  return 0;
}

static int usart2_drv_write(const uint8_t *buf, uint32_t len) {
  for (uint32_t i = 0; i < len; i++)
    usart2_putc((char)buf[i]);
  return (int)len;
}

static int usart2_drv_write_sync(const uint8_t *buf, uint32_t len) {
  for (uint32_t i = 0; i < len; i++)
    usart2_fault_putc((char)buf[i]);
  return (int)len;
}

static int usart2_drv_read(uint8_t *buf, uint32_t len) {
  uint32_t n = 0;
  char c;
  while (n < len && usart2_getc(&c))
    buf[n++] = (uint8_t)c;
  return (int)n;
}

const driver_t usart2_driver = {.name = "usart2",
                                .init = usart2_drv_init,
                                .write = usart2_drv_write,
                                .write_sync = usart2_drv_write_sync,
                                .read = usart2_drv_read};
