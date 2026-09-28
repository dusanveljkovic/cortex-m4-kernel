#ifndef _DRIVER_H
#define _DRIVER_H

#include "stdint.h"

typedef struct driver {
  const char *name;

  int (*init)(void);

  int (*write)(const uint8_t *buf, uint32_t len);

  int (*write_sync)(const uint8_t *buf, uint32_t len);

  int (*read)(uint8_t *buf, uint32_t len);
} driver_t;

#define DRIVER_MAX 8

int driver_register(const driver_t *drv);
const driver_t *driver_find(const char *name);

#endif
