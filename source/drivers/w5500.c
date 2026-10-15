#include "w5500.h"
#include "../../include/gpio.h"
#include "../../include/kprintf.h"
#include "../../include/rcc.h"
#include "../../include/syscall.h"
#include "spi.h"

#define W5500_RESET_PIN 1

#define W5500_BSB_COMMON 0x00

#define W5500_CTRL_RWB_READ (0 << 2)
#define W5500_CTRL_RWB_WRITE (1 << 2)
#define W5500_CTRL_OM_VDM (0b00 << 0)

static spi_return_t w5500_transfer(uint16_t addr, uint8_t bsb,
                                   spi1_frame_t *frame) {
  uint8_t control =
      (bsb << 3) | W5500_CTRL_OM_VDM |
      (frame->is_read == true ? W5500_CTRL_RWB_READ : W5500_CTRL_RWB_WRITE);
  uint8_t h[3];
  h[0] = (addr >> 8) & 0xFF;
  h[1] = (addr & 0xFF);
  h[2] = control;

  spi1_frame_t my_frame = {.header = h,
                           .header_len = 3,
                           .tx_data = frame->tx_data,
                           .rx_data = frame->rx_data,
                           .is_read = frame->is_read,
                           .data_len = frame->data_len};

  return spi1_transfer_frame(&my_frame);
}

static spi_return_t w5500_read8(uint16_t addr, uint8_t bsb, uint8_t *data) {
  spi1_frame_t frame = {.is_read = true, .rx_data = data, .data_len = 1};
  return w5500_transfer(addr, bsb, &frame);
}

static spi_return_t w5500_write8(uint16_t addr, uint8_t bsb, uint8_t v) {
  spi1_frame_t frame = {.is_read = false, .tx_data = &v, .data_len = 1};
  return w5500_transfer(addr, bsb, &frame);
}

static spi_return_t w5500_read16(uint16_t addr, uint8_t bsb, uint16_t *data) {
  uint8_t v[2] = {0, 0};
  spi1_frame_t frame = {.is_read = true, .rx_data = v, .data_len = 2};
  spi_return_t ret = w5500_transfer(addr, bsb, &frame);
  *data = ((uint16_t)v[0] << 8) | v[1];
  return ret;
}

static spi_return_t w5500_read16_stable(uint16_t addr, uint8_t bsb) {
  uint16_t a, b;
  spi_return_t ret = SPI_OK;
  do {
    ret = w5500_read16(addr, bsb, &a);
    if (ret != SPI_OK)
      break;
    ret = w5500_read16(addr, bsb, &b);
    if (ret != SPI_OK)
      break;
  } while (a != b);
  return ret;
}

static spi_return_t w5500_write16(uint16_t addr, uint8_t bsb, uint16_t data) {
  uint8_t v[2];
  v[0] = (uint8_t)(data >> 8);
  v[1] = (uint8_t)(data & 0x00FF);
  spi1_frame_t frame = {.is_read = false, .tx_data = v, .data_len = 2};
  return w5500_transfer(addr, bsb, &frame);
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

#define INIT_ATTEMPTS 5

void w5500_init(const uint8_t mac[6], const uint8_t ip[4],
                const uint8_t gateway[4], const uint8_t subnet[4]) {
  uint8_t attempts = 0;
  uint8_t version = 0;
  spi_return_t ret = SPI_OK;
  do {
    w5500_reset_pin_init();
    w5500_hw_reset();
    ret = w5500_get_version(&version);
    if (ret != SPI_OK)
      kpanicf("[w5500] SPI error: %d\r\n", ret);
    kpanicf("[w5500] trying to start, attempt %d\r\n", attempts);
  } while (version != 0x04 && ++attempts < INIT_ATTEMPTS);

  kpanicf("[w5500] version %d\r\n", version);
  if (attempts == INIT_ATTEMPTS) {
    kpanicf("[!][w5500] couldn't start\r\n");
    return;
  }

  spi1_frame_t mac_frame = {.is_read = false, .tx_data = mac, .data_len = 6};
  spi1_frame_t ip_frame = {.is_read = false, .tx_data = ip, .data_len = 4};
  spi1_frame_t gateway_frame = {
      .is_read = false, .tx_data = gateway, .data_len = 4};
  spi1_frame_t subnet_frame = {
      .is_read = false, .tx_data = subnet, .data_len = 4};
  ret = w5500_transfer(W5500_SHAR, W5500_BSB_COMMON, &mac_frame);
  ret = w5500_transfer(W5500_SIPR, W5500_BSB_COMMON, &ip_frame);
  ret = w5500_transfer(W5500_GAR, W5500_BSB_COMMON, &gateway_frame);
  ret = w5500_transfer(W5500_SUBR, W5500_BSB_COMMON, &subnet_frame);
}

spi_return_t w5500_get_version(uint8_t *data) {
  return w5500_read8(W5500_VERSIONR, W5500_BSB_COMMON, data);
}

spi_return_t w5500_link_up(int *up) {
  uint8_t data = 0;
  spi_return_t ret = w5500_read8(W5500_PHYCFGR, W5500_BSB_COMMON, &data);
  if (ret == SPI_OK)
    *up = data;
  else
    *up = -1;

  return ret;
}
