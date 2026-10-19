#include "spi.h"
#include "../../include/gpio.h"
#include "../../include/kprintf.h"
#include "../../include/rcc.h"
#include "../../include/semaphore.h"
#include "../../include/syscall.h"

#define TRANSFER_TIMEOUT 100000

static void spi1_cs_select(void) { GPIO_PIN_LOW(GPIOA, SPI1_PIN_CS); }
static void spi1_cs_deselect(void) { GPIO_PIN_HIGH(GPIOA, SPI1_PIN_CS); }

static mutex_t spi1_lock = {0, 0, 0, 0};
static bool initialized = false;

void spi1_init(void) {
  if (initialized) {
    kpanicf("[!][spi1] already initialized\r\n");
    return;
  }
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
  RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;

  SET_GPIO_MODE(GPIOA, GPIO_MODE_OUTPUT, SPI1_PIN_CS);
  SET_GPIO_OTYPER(GPIOA, 0, SPI1_PIN_CS);
  SET_GPIO_OSPEEDR(GPIOA, GPIO_OSPEEDR_FAST, SPI1_PIN_CS);
  spi1_cs_deselect();

  uint8_t pins[] = {SPI1_PIN_SCK, SPI1_PIN_MISO, SPI1_PIN_MOSI};
  for (int i = 0; i < 3; i++) {
    SET_GPIO_MODE(GPIOA, GPIO_MODE_AF, pins[i]);
    SET_GPIO_OTYPER(GPIOA, 0, pins[i]);
    SET_GPIO_OSPEEDR(GPIOA, GPIO_OSPEEDR_FAST, pins[i]);
    SET_GPIO_PUPDR(GPIOA, GPIO_PUPDR_NOPP, pins[i]);
    SET_GPIO_AF(GPIOA, SPI1_AF, pins[i])
  }

  SPI1->CR1 = 0;
  SPI1->CR1 = SPI_CR1_MSTR | SPI_CR1_BR_DIV8 // 84MHz / 8 = 10.5 MHz clock
              | SPI_CR1_SSM | SPI_CR1_SSI;

  SPI1->CR2 = 0;

  SPI1->CR1 |= SPI_CR1_SPE;

  mutex_init(&spi1_lock);
  initialized = true;
  kpanicf("[spi1] successfully initialized\r\n");
}

static spi_return_t spi1_transfer(uint8_t data, uint8_t *received) {
  uint32_t i = 0;
  while (!(SPI1->SR & SPI_SR_TXE)) {
    if (++i >= TRANSFER_TIMEOUT)
      return SPI_ERR_TXE;
  }
  i = 0;

  SPI1->DR = data;

  while (!(SPI1->SR & SPI_SR_RXNE)) {
    if (++i >= TRANSFER_TIMEOUT)
      return SPI_ERR_RXE;
  }

  uint8_t spi_dr = (uint8_t)SPI1->DR;
  if (received != 0)
    *received = spi_dr;
  return SPI_OK;
}

spi_return_t spi1_transfer_frame(spi1_frame_t *frame) {
  spi_return_t transfer_ret = SPI_OK;

  sys_mutex_lock(&spi1_lock);
  spi1_cs_select();

  for (uint32_t i = 0; i < frame->header_len; i++) {
    transfer_ret = spi1_transfer(frame->header[i], 0);
    if (transfer_ret != SPI_OK)
      break;
  }

  if (transfer_ret == SPI_OK) {
    for (uint32_t i = 0; i < frame->data_len; i++) {
      uint8_t received;
      transfer_ret =
          spi1_transfer(frame->is_read ? 0xFF : frame->tx_data[i], &received);
      if (transfer_ret != SPI_OK)
        break;

      if (frame->is_read && frame->rx_data)
        frame->rx_data[i] = received;
    }
  }

  spi1_cs_deselect();
  sys_mutex_unlock(&spi1_lock);

  return transfer_ret;
}
