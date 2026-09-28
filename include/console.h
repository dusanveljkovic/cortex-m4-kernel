#ifndef _CONSOLE_H
#define _CONSOLE_H

#include "driver.h"
#include "stdint.h"

#define CONSOLE_MAX_SINKS 4

int console_attach(const driver_t *drv);
int console_detach(const driver_t *drv);

void console_write(const char *buf, uint32_t len);

void console_write_sync(const char *buf, uint32_t len);

int console_read(uint8_t *buf, uint32_t len);

#endif
