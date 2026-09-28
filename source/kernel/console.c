#include "../../include/console.h"
#include "../../include/arm.h"

static const driver_t *sinks[CONSOLE_MAX_SINKS];

int console_attach(const driver_t *drv) {
  if (drv == 0)
    return -1;

  uint32_t irq_state = irq_save();
  int free_slot = -1;
  for (int i = 0; i < CONSOLE_MAX_SINKS; i++) {
    if (sinks[i] == drv) {
      irq_restore(irq_state);
      return 0;
    }
    if (sinks[i] == 0 && free_slot < 0)
      free_slot = i;
  }
  if (free_slot >= 0)
    sinks[free_slot] = drv;
  irq_restore(irq_state);

  return free_slot >= 0 ? 0 : -2;
}

int console_detach(const driver_t *drv) {
  uint32_t irq_state = irq_save();
  int free_slot = -1;
  for (int i = 0; i < CONSOLE_MAX_SINKS; i++) {
    if (sinks[i] == drv)
      sinks[i] = 0;
  }
  irq_restore(irq_state);

  return 0;
}

void console_write(const char *buf, uint32_t len) {
  for (int i = 0; i < CONSOLE_MAX_SINKS; i++) {
    const driver_t *sink = sinks[i];
    if (sink != 0 && sink->write != 0)
      sink->write((const uint8_t *)buf, len);
  }
}

void console_write_sync(const char *buf, uint32_t len) {
  for (int i = 0; i < CONSOLE_MAX_SINKS; i++) {
    const driver_t *sink = sinks[i];
    if (sink != 0 && sink->write_sync != 0)
      sink->write_sync((const uint8_t *)buf, len);
  }
}

int console_read(uint8_t *buf, uint32_t len) {
  for (int i = 0; i < CONSOLE_MAX_SINKS; i++) {
    const driver_t *sink = sinks[i];
    if (sink != 0 && sink->read != 0) {
      int n = sink->read(buf, len);
      if (n > 0)
        return n;
    }
  }
  return 0;
}
