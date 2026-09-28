#include "w5500.h"
#include "../../include/gpio.h"
#include "../../include/rcc.h"
#include "../../include/syscall.h"
#include "spi.h"

#define W5500_RESET_PIN 1

#define W5500_BSB_COMMON 0x00

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
  return ((uint16_t)v[0] << 8) | v[1];
}

static uint16_t w5500_read16_stable(uint16_t addr, uint8_t bsb) {
  uint16_t a, b;
  do {
    a = w5500_read16(addr, bsb);
    b = w5500_read16(addr, bsb);
  } while (a != b);
  return a;
}

static void w5500_write16(uint16_t addr, uint8_t bsb, uint16_t data) {
  uint8_t v[2];
  v[0] = (uint8_t)(data >> 8);
  v[1] = (uint8_t)(data & 0x00FF);
  w5500_write(addr, bsb, v, 2);
}

static void w5500_reset_pin_init(void) {
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
  SET_GPIO_MODE(GPIOA, GPIO_MODE_OUTPUT, W5500_RESET_PIN);
  SET_GPIO_OTYPER(GPIOA, 0, W5500_RESET_PIN);
  SET_GPIO_OSPEEDR(GPIOA, GPIO_OSPEEDR_FAST, W5500_RESET_PIN);
  GPIO_PIN_HIGH(GPIOA, W5500_RESET_PIN); // idle high
}

static void w5500_hw_reset(void) {
  GPIO_PIN_LOW(GPIOA, W5500_RESET_PIN);
  sys_sleep(1);
  GPIO_PIN_HIGH(GPIOA, W5500_RESET_PIN);
  sys_sleep(50);
}

void w5500_init(const uint8_t mac[6], const uint8_t ip[4],
                const uint8_t gateway[4], const uint8_t subnet[4]) {
  spi1_init();
  w5500_reset_pin_init();
  w5500_hw_reset();

  w5500_write(W5500_SHAR, W5500_BSB_COMMON, mac, 6);
  w5500_write(W5500_SIPR, W5500_BSB_COMMON, ip, 4);
  w5500_write(W5500_GAR, W5500_BSB_COMMON, gateway, 4);
  w5500_write(W5500_SUBR, W5500_BSB_COMMON, subnet, 4);
}

uint8_t w5500_get_version(void) {
  return w5500_read8(W5500_VERSIONR, W5500_BSB_COMMON);
}

bool w5500_link_up(void) {
  return (w5500_read8(W5500_PHYCFGR, W5500_BSB_COMMON) & 0x01) != 0;
}
