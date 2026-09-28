#include "../../include/driver.h"
#include "../../include/arm.h"
#include "../../include/utils.h"

static const driver_t *registry[DRIVER_MAX];

int driver_register(const driver_t *drv) {
  if (drv == 0 || drv->name == 0)
    return -1;

  if (drv->init != 0 && drv->init() != 0)
    return -2;

  uint32_t irq_state = irq_save();
  for (int i = 0; i < DRIVER_MAX; i++) {
    if (registry[i] == 0) {
      registry[i] = drv;
      irq_restore(irq_state);
      return 0;
    }
  }
  irq_restore(irq_state);
  return -3;
}

const driver_t *driver_find(const char *name) {
  for (int i = 0; i < DRIVER_MAX; i++) {
    if (registry[i] != 0 && strcmp(registry[i]->name, name) == 0)
      return registry[i];
  }
  return 0;
}
