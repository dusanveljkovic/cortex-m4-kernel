#include "spi.h"
#include "../../include/gpio.h"
#include "../../include/rcc.h"
#include "../../include/semaphore.h"
#include "../../include/syscall.h"

static void spi1_cs_select(void) { GPIO_PIN_LOW(GPIOA, SPI1_PIN_CS); }
static void spi1_cs_deselect(void) { GPIO_PIN_HIGH(GPIOA, SPI1_PIN_CS); }

static mutex_t spi1_lock = {0, 0, 0, 0};

void spi1_init(void) {
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
}

static uint8_t spi1_transfer(uint8_t data) {
  while (!(SPI1->SR & SPI_SR_TXE))
    ;

  SPI1->DR = data;

  while (!(SPI1->SR & SPI_SR_RXNE))
    ;

  return (uint8_t)SPI1->DR;
}

void spi1_transfer_frame(spi1_frame_t *frame) {
  sys_mutex_lock(&spi1_lock);

  spi1_cs_select();

  for (uint32_t i = 0; i < frame->header_len; i++) {
    spi1_transfer(frame->header[i]);
  }

  for (uint32_t i = 0; i < frame->data_len; i++) {
    uint8_t received = spi1_transfer(frame->tx_data ? frame->tx_data[i] : 0xFF);
    if (frame->rx_data)
      frame->rx_data[i] = received;
  }

  spi1_cs_deselect();

  sys_mutex_unlock(&spi1_lock);
}
