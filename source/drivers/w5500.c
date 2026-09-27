#include "w5500.h"
#include "../../include/gpio.h"
#include "../../include/rcc.h"
#include "spi.h"

#define W5500_CTRL_RWB_READ (0 << 2)
#define W5500_CTRL_RWB_WRITE (1 << 2)
#define W5500_CTRL_OM_VDM (0b00 << 0)

static void w5500_write(uint16_t addr, uint8_t bsb, const uint8_t *data,
                        uint16_t len) {
  uint8_t control = (bsb << 3) | W5500_CTRL_RWB_WRITE | W5500_CTRL_OM_VDM;

  spi1_cs_select();
  spi1_transfer((addr >> 8) & 0xFF);
  spi1_transfer(addr & 0xFF);
  spi1_transfer(control);
  spi1_write(data, len);
  spi1_cs_deselect();
}

static void w5500_read(uint16_t addr, uint8_t bsb, uint8_t *data,
                       uint16_t len) {
  uint8_t control = (bsb << 3) | W5500_CTRL_RWB_READ | W5500_CTRL_OM_VDM;

  spi1_cs_select();
  spi1_transfer((addr >> 8) & 0xFF);
  spi1_transfer(addr & 0xFF);
  spi1_transfer(control);
  spi1_read(data, len);
  spi1_cs_deselect();
}

static uint8_t w5500_read8(uint16_t addr, uint8_t bsb) {
  uint8_t v;
  w5500_read(addr, bsb, &v, 1);
  return v;
}

static void w5500_write8(uint16_t addr, uint8_t bsb, uint8_t v) {
  w5500_write(addr, bsb, &v, 1);
}

static uint16_t w5500_read16(uint16_t addr, uint8_t bsb) {
  uint8_t v[2];
  w5500_read(addr, bsb, v, 2);
  return ((uint16_t)v[0] << 16) | v[1];
}

static uint16_t w5500_read16_stable(uint16_t addr, uint8_t bsb) {
  uint16_t a, b;
  do {
    a = w5500_read16(addr, bsb);
    b = w5500_read16(addr, bsb);
  } while (a != b);
  return a;
}
